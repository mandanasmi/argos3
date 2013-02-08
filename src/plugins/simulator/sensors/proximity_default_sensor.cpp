/**
 * @file <argos3/plugins/robots/foot-bot/simulator/ring_proximity_sensor.cpp>
 *
 * @author Carlo Pinciroli - <ilpincy@gmail.com>
 */

#include <argos3/core/simulator/entity/embodied_entity.h>
#include <argos3/core/simulator/entity/composable_entity.h>
#include <argos3/core/simulator/simulator.h>

#include "proximity_default_sensor.h"

namespace argos {

   /****************************************/
   /****************************************/

   static CRange<Real> UNIT(0.0f, 0.1f);

   /****************************************/
   /****************************************/

   CProximityDefaultSensor::CProximityDefaultSensor() :
      m_pcEmbodiedEntity(NULL),
      m_bShowRays(false),
      m_pcRNG(NULL),
      m_bAddNoise(false),
      m_cSpace(CSimulator::GetInstance().GetSpace()),
      m_cEmbodiedSpaceHash(m_cSpace.GetEmbodiedEntitiesSpaceHash()) {}

   /****************************************/
   /****************************************/

   CProximityDefaultSensor::~CProximityDefaultSensor() {
      while(!m_vecSensors.empty()) {
         delete m_vecSensors.back();
         m_vecSensors.pop_back();
      }
   }

   /****************************************/
   /****************************************/

   void CProximityDefaultSensor::SetRobot(CComposableEntity& c_entity) {
      m_pcEmbodiedEntity = &(c_entity.GetComponent<CEmbodiedEntity>("body"));
      m_pcControllableEntity = &(c_entity.GetComponent<CControllableEntity>("controller"));
      /* Ignore the sensing robot when checking for occlusions */
      m_tIgnoreMe.insert(m_pcEmbodiedEntity);
   }

   /****************************************/
   /****************************************/

   void CProximityDefaultSensor::Init(TConfigurationNode& t_tree) {
      try {
         CCI_ProximitySensor::Init(t_tree);
         /* Show rays? */
         GetNodeAttributeOrDefault(t_tree, "show_rays", m_bShowRays, m_bShowRays);
         /* Parse noise level */
         Real fNoiseLevel = 0.0f;
         GetNodeAttributeOrDefault(t_tree, "noise_level", fNoiseLevel, fNoiseLevel);
         if(fNoiseLevel < 0.0f) {
            THROW_ARGOSEXCEPTION("Can't specify a negative value for the noise level of the proximity sensor");
         }
         else if(fNoiseLevel > 0.0f) {
            m_bAddNoise = true;
            m_cNoiseRange.Set(-fNoiseLevel, fNoiseLevel);
            m_pcRNG = CRandom::CreateRNG("argos");
         }
         /* Parse proximity sensors */
         if(t_tree.NoChildren()) {
            THROW_ARGOSEXCEPTION("No sensors defined");
         }
         TConfigurationNodeIterator it;
         CVector3 cPos, cDir;
         Real fRange;
         for(it = it.begin(&t_tree); it != it.end(); ++it) {
            if(it->Value() != "sensor") {
               THROW_ARGOSEXCEPTION("Parse error: expected <sensor />, read \"" << it->Value() << "\"");
            }
            GetNodeAttribute(*it, "position", cPos);
            GetNodeAttribute(*it, "direction", cDir);
            GetNodeAttribute(*it, "range", fRange);
            m_vecSensors.push_back(new SSensor(cPos, cDir, fRange));
         }
         m_tReadings.resize(m_vecSensors.size());
      }
      catch(CARGoSException& ex) {
         THROW_ARGOSEXCEPTION_NESTED("Initialization error in default proximity sensor", ex);
      }
   }

   /****************************************/
   /****************************************/
   
   void CProximityDefaultSensor::Update() {
      /* Ray used for scanning the environment for obstacles */
      CRay cScanningRay;
      CVector3 cRayStart, cRayEnd;
      /* Buffers to contain data about the intersection */
      CSpace::SEntityIntersectionItem<CEmbodiedEntity> sIntersection;
      /* Go through the sensors */
      for(UInt32 i = 0; i < m_vecSensors.size(); ++i) {
         /* Compute ray for sensor i */
         cRayStart = m_vecSensors[i]->Position;
         cRayStart.Rotate(m_pcEmbodiedEntity->GetOrientation());
         cRayStart += m_pcEmbodiedEntity->GetPosition();
         cRayEnd = m_vecSensors[i]->Position;
         cRayEnd += m_vecSensors[i]->Direction;
         cRayEnd.Rotate(m_pcEmbodiedEntity->GetOrientation());
         cRayEnd += m_pcEmbodiedEntity->GetPosition();
         cScanningRay.Set(cRayStart,cRayEnd);
         /* Compute reading */
         /* Get the closest intersection */
         if(m_cSpace.GetClosestEmbodiedEntityIntersectedByRay(sIntersection,
                                                              cScanningRay,
                                                              m_tIgnoreMe)) {
            /* There is an intersection */
            if(m_bShowRays) {
               m_pcControllableEntity->AddIntersectionPoint(cScanningRay,
                                                            sIntersection.TOnRay);
               m_pcControllableEntity->AddCheckedRay(true, cScanningRay);
            }
            CalculateReading(cScanningRay.GetDistance(sIntersection.TOnRay));
         }
         else {
            /* No intersection */
            m_tReadings[i] = 0.0f;
            if(m_bShowRays) {
               m_pcControllableEntity->AddCheckedRay(false, cScanningRay);
            }
         }
         /* Apply noise to the sensor */
         if(m_bAddNoise) {
            m_tReadings[i] += m_pcRNG->Uniform(m_cNoiseRange);
         }
         /* Trunc the reading between 0 and 1 */
         UNIT.TruncValue(m_tReadings[i]);
      }
   }

   /****************************************/
   /****************************************/

   void CProximityDefaultSensor::Reset() {
      for(UInt32 i = 0; i < GetReadings().size(); ++i) {
         m_tReadings[i] = 0.0f;
      }
   }

   /****************************************/
   /****************************************/

   Real CProximityDefaultSensor::CalculateReading(Real f_distance) {
      return Exp(-f_distance);
   }

   /****************************************/
   /****************************************/

   REGISTER_SENSOR(CProximityDefaultSensor,
                   "proximity", "default",
                   "Carlo Pinciroli [ilpincy@gmail.com]",
                   "1.0",
                   "A generic, configurable proximity sensor",
                   "This sensor accesses a set of proximity sensors. The sensors all return a value\n"
                   "between 0 and 1, where 0 means nothing within range and 1 means an external\n"
                   "object is touching the sensor. Values between 0 and 1 depend on the distance of\n"
                   "the occluding object, and are calculated as value=exp(-distance). In\n"
                   "controllers, you must include the ci_proximity_sensor.h header.\n\n"
                   "REQUIRED XML CONFIGURATION\n\n"
                   "  <controllers>\n"
                   "    ...\n"
                   "    <my_controller ...>\n"
                   "      ...\n"
                   "      <sensors>\n"
                   "        ...\n"
                   "        <proximity implementation=\"default\">\n"
                   "          <sensor position=\"0.1,0,0.1\" direction=\"1,0,0\" range=\"0.5\" />\n"
                   "          <sensor position=\"0,0.1,0.1\" direction=\"0,1,0\" range=\"0.5\" />\n"
                   "          ...\n"
                   "        </proximity>\n"
                   "        ...\n"
                   "      </sensors>\n"
                   "      ...\n"
                   "    </my_controller>\n"
                   "    ...\n"
                   "  </controllers>\n\n"
                   "Each <sensor> tag sets the position of a new sensor. For each sensor, one must\n"
                   "assign its offset with respect to the position of the robot's embodied entity\n"
                   "(attribute \"position\"), the direction towards which the proximity sensor casts\n"
                   "its ray (attribute \"direction\"), and range, i.e., the length of the ray\n"
                   "(attribute \"range\"). The direction can be not normalized, because\n"
                   "normalization is performed internally.\n\n"
                   "OPTIONAL XML CONFIGURATION\n\n"
                   "It is possible to draw the rays shot by the proximity sensor in the OpenGL\n"
                   "visualization. This can be useful for sensor debugging but also to understand\n"
                   "what's wrong in your controller. In OpenGL, the rays are drawn in cyan when\n"
                   "they are not obstructed and in purple when they are. In case a ray is\n"
                   "obstructed, a black dot is drawn where the intersection occurred.\n"
                   "To turn this functionality on, add the attribute \"show_rays\" as in this\n"
                   "example:\n\n"
                   "  <controllers>\n"
                   "    ...\n"
                   "    <my_controller ...>\n"
                   "      ...\n"
                   "      <sensors>\n"
                   "        ...\n"
                   "        <proximity implementation=\"default\"\n"
                   "                   show_rays=\"true\">\n"
                   "          <sensor position=\"0.1,0,0.1\" direction=\"1,0,0\" range=\"0.5\" />\n"
                   "          <sensor position=\"0,0.1,0.1\" direction=\"0,1,0\" range=\"0.2\" />\n"
                   "          ...\n"
                   "        </proximity>\n"
                   "        ...\n"
                   "      </sensors>\n"
                   "      ...\n"
                   "    </my_controller>\n"
                   "    ...\n"
                   "  </controllers>\n\n"
                   "It is possible to add uniform noise to the sensors, thus matching the\n"
                   "characteristics of a real robot better. This can be done with the attribute\n"
                   "\"noise_level\", whose allowed range is in [-1,1] and is added to the calculated\n"
                   "reading. The final sensor reading is always normalized in the [0-1] range.\n\n"
                   "  <controllers>\n"
                   "    ...\n"
                   "    <my_controller ...>\n"
                   "      ...\n"
                   "      <sensors>\n"
                   "        ...\n"
                   "        <proximity implementation=\"default\"\n"
                   "                   noise_level=\"0.1\">\n"
                   "          <sensor position=\"0.1,0,0.1\" direction=\"1,0,0\" range=\"0.5\" />\n"
                   "          <sensor position=\"0,0.1,0.1\" direction=\"0,1,0\" range=\"0.2\" />\n"
                   "          ...\n"
                   "        </proximity>\n"
                   "        ...\n"
                   "      </sensors>\n"
                   "      ...\n"
                   "    </my_controller>\n"
                   "    ...\n"
                   "  </controllers>",
                   "Usable"
		  );

}