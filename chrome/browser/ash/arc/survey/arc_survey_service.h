// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SURVEY_ARC_SURVEY_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_SURVEY_ARC_SURVEY_SERVICE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class HatsNotificationController;
}  // namespace ash

namespace arc {

class ArcBridgeService;
class ArcSurveyServiceTest;

// This service monitors as ARC apps are created/destroyed and determines when
// to show ARC++ Games Survey.
class ArcSurveyService : public KeyedService, public ArcAppListPrefs::Observer {
 public:
  // Map: <package_name, <number entries in |TaskIdMap|, timestamp)>
  using PackageNameMap = std::map<std::string, std::pair<int, base::Time>>;
  // Map: <task_id, package_name>
  using TaskIdMap = std::map<int32_t, std::string>;

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSurveyService* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcSurveyService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcSurveyService(content::BrowserContext* context,
                   ArcBridgeService* arc_bridge_service);

  ArcSurveyService(const ArcSurveyService&) = delete;
  ArcSurveyService& operator=(const ArcSurveyService&) = delete;
  ~ArcSurveyService() override;

  // ArcAppListPrefs::Observer:
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;

  // ArcAppListPrefs::Observer:
  void OnTaskDestroyed(int32_t task_id) override;

  // ArcAppListPrefs::Observer:
  void OnArcAppListPrefsDestroyed() override;

  const PackageNameMap* GetPackageNameMapForTesting();
  const TaskIdMap* GetTaskIdMapForTesting();
  const std::set<std::string>* GetAllowedPackagesForTesting();
  void AddAllowedPackageNameForTesting(const std::string package_name);

  static void EnsureFactoryBuilt();

 private:
  friend class ArcSurveyServiceTest;
  bool LoadSurveyData(std::string survey_data);

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observer_{this};

  // These 2 maps are updated when ArcAppListPrefs::Observer's "OnTaskDestroyed"
  // and "OnTaskCreated" methods are called. A |task_id| maps uniquely to a
  // |package_name|, which is managed by |task_id_map_|. The reverse unique
  // mapping is not true, because a |package_name| can create multiple
  // |task_id|s. |package_name_map_| manages the reverse mapping along with some
  // extra info. See |package_name_map_| for details. Because the mapping isn't
  // unique, this 2 map model is needed. The 2 maps are updated ONLY when the 2
  // observer methods are called.

  // Contains a |task_id| as the key, and |package_name| as the value. The map's
  // value is used as a key in |package_name_map_|.
  TaskIdMap task_id_map_;

  // Contains a |package_name| as the key and a data pair as the value.
  // The data pair's first value is the number entries in |task_id_map_| that
  // has |package_name| as its value. The second value is the timestamp of when
  // |OnTaskCreated| was first called. I
  PackageNameMap package_name_map_;

  // List of package names for which to show the survey.
  std::set<std::string> allowed_packages_;

  // Minimum time an app needs to have run before showing the ARC Games survey.
  base::TimeDelta elapsed_time_survey_trigger_;

  // Unowned pointer.
  const raw_ptr<Profile> profile_;

  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SURVEY_ARC_SURVEY_SERVICE_H_
