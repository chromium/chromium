// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/survey/arc_survey_service.h"

#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

namespace {

const int kArcGameSurveyTriggerTimeInMinutes = 10;

// Singleton factory for ArcSurveyServiceFactory.
class ArcSurveyServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSurveyService,
          ArcSurveyServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSurveyServiceFactory";

  static ArcSurveyServiceFactory* GetInstance() {
    return base::Singleton<ArcSurveyServiceFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcSurveyServiceFactory>;
  ArcSurveyServiceFactory() = default;
  ~ArcSurveyServiceFactory() override = default;
};

}  // namespace

// static
ArcSurveyService* ArcSurveyService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSurveyServiceFactory::GetForBrowserContext(context);
}

// static
ArcSurveyService* ArcSurveyService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSurveyServiceFactory::GetForBrowserContextForTesting(context);
}

ArcSurveyService::ArcSurveyService(content::BrowserContext* context,
                                   ArcBridgeService* arc_bridge_service) {
  VLOG(1) << "ArcSurveyService created";
  ArcAppListPrefs* prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(context));
  if (prefs)
    arc_prefs_observer_.Observe(prefs);
  // TODO(b:204572472): Populate |allowed_packages_| from
  // HatsConfig::kHatsArcGamesSurvey.
}

ArcSurveyService::~ArcSurveyService() {
  package_name_map_.clear();
  task_id_map_.clear();
}

void ArcSurveyService::OnTaskCreated(int32_t task_id,
                                     const std::string& package_name,
                                     const std::string& activity,
                                     const std::string& intent,
                                     int32_t session_id) {
  if (allowed_packages_.count(package_name) == 0) {
    // Skip |package_name|.
    return;
  }

  // Add |task_id| to TaskIdMap
  task_id_map_.emplace(task_id, package_name);

  // Add or update the respective entry in PackageNameMap
  PackageNameMap::iterator entry = package_name_map_.find(package_name);
  if (entry == package_name_map_.end()) {
    // Add new entry
    package_name_map_.emplace(
        package_name, std::make_pair(1, base::Time::NowFromSystemTime()));
  } else {
    // Update the count for the existing entry.
    entry->second.first++;
  }
}

void ArcSurveyService::OnTaskDestroyed(int32_t task_id) {
  // Remove |task_id| from TaskIdMap
  const TaskIdMap::const_iterator task_id_iterator = task_id_map_.find(task_id);
  if (task_id_iterator == task_id_map_.end()) {
    // Not monitoring the |task_id|
    VLOG(1) << "Returning. " << task_id << " is not in TaskIDMap";
    return;
  }
  const std::string package_name = task_id_iterator->second;
  task_id_map_.erase(task_id_iterator);

  // Update PackageNameMap
  const PackageNameMap::iterator package_name_iterator =
      package_name_map_.find(package_name);
  if (package_name_iterator == package_name_map_.end()) {
    DVLOG(1) << "Unable to find \"" << package_name << "\" in PackageNameMap";
    return;
  }
  package_name_iterator->second.first--;  // Decrement count
  if (package_name_iterator->second.first == 0) {
    const base::Time on_task_create_timestamp =
        package_name_iterator->second.second;
    package_name_map_.erase(package_name_iterator);

    // Trigger ArcGames survey if it's been active for at least
    // |kArcGameSurveyTriggerTimeInMinutes|.
    base::TimeDelta elapsedTime =
        base::Time::NowFromSystemTime() - on_task_create_timestamp;
    if (elapsedTime.InMinutes() >= kArcGameSurveyTriggerTimeInMinutes) {
      VLOG(1) << "~~~ Elapsed time is more than 10: "
              << elapsedTime.InMinutes();
      // TODO(b:204572472): Trigger HatsConfig::kHatsArcGamesSurvey
    }
  }
}

void ArcSurveyService::OnArcAppListPrefsDestroyed() {
  arc_prefs_observer_.Reset();
}

const ArcSurveyService::PackageNameMap*
ArcSurveyService::GetPackageNameMapForTesting() {
  return &package_name_map_;
}

const ArcSurveyService::TaskIdMap* ArcSurveyService::GetTaskIdMapForTesting() {
  return &task_id_map_;
}

const std::set<std::string>* ArcSurveyService::GetAllowedPackagesForTesting() {
  return &allowed_packages_;
}

void ArcSurveyService::AddAllowedPackageNameForTesting(
    const std::string package_name) {
  allowed_packages_.emplace(package_name);
}

}  // namespace arc
