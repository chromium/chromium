// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/survey/arc_survey_service.h"

#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_finch_helper.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

namespace {

const base::TimeDelta kArcGameElapsedTimeSurveyTrigger = base::Minutes(10);
constexpr char kJSONKeyElapsedTimeSurveyTriggerMin[] =
    "elapsed_time_survey_trigger_min";
constexpr char kJSONKeyPackageNames[] = "package_names";
constexpr char kKeyMostRecentAndroidGame[] = "mostRecentAndroidGame";

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
                                   ArcBridgeService* arc_bridge_service)
    : elapsed_time_survey_trigger_(kArcGameElapsedTimeSurveyTrigger),
      profile_(Profile::FromBrowserContext(context)) {
  DVLOG(1) << "ArcSurveyService created";

  if (!base::FeatureList::IsEnabled(ash::kHatsArcGamesSurvey.feature)) {
    VLOG(1) << "ARC Games HaTS survey feature is not enabled";
    return;
  }
  std::string survey_data = ash::HatsFinchHelper::GetCustomClientDataAsString(
      ash::kHatsArcGamesSurvey);
  if (survey_data.length() == 0) {
    VLOG(1) << "No survey data found for ARC Games.";
    return;
  }
  if (!LoadSurveyData(survey_data)) {
    VLOG(1) << "Error loading the survey data.";
    return;
  }
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs)
    arc_prefs_observer_.Observe(prefs);
}

ArcSurveyService::~ArcSurveyService() {
  package_name_map_.clear();
  task_id_map_.clear();
}

bool ArcSurveyService::LoadSurveyData(std::string survey_data) {
  std::optional<base::Value> root = base::JSONReader::Read(survey_data);
  if (!root) {
    LOG(ERROR) << "Unable to find JSON root. Trying char substitutions.";
    base::ReplaceSubstringsAfterOffset(&survey_data, 0, R"(\{@})", ":");
    base::ReplaceSubstringsAfterOffset(&survey_data, 0, R"(\{~})", ",");
    base::ReplaceSubstringsAfterOffset(&survey_data, 0, R"(\{%})", ".");
    DVLOG(1) << "Data after substitution: " << survey_data;
    root = base::JSONReader::Read(survey_data);
    if (!root) {
      LOG(ERROR) << "Unable to find JSON root after substitution";
      return false;
    }
  }

  // Load trigger duration
  std::optional<int> elapsed_time_survey_trigger_min =
      root->GetDict().FindInt(kJSONKeyElapsedTimeSurveyTriggerMin);
  if (elapsed_time_survey_trigger_min) {
    elapsed_time_survey_trigger_ =
        (elapsed_time_survey_trigger_min.value() >= 0)
            ? base::Minutes(elapsed_time_survey_trigger_min.value())
            : kArcGameElapsedTimeSurveyTrigger;
    DVLOG(1) << "Survey elapsed time trigger: " << elapsed_time_survey_trigger_;
  }

  // Load package names
  const base::Value::List* list =
      root->GetDict().FindList(kJSONKeyPackageNames);
  if (!list) {
    VLOG(1) << "List of package names not found in the survey data.";
    return false;
  }
  if (list->empty()) {
    VLOG(1) << "List of package names is empty in the survey data.";
    return false;
  }
  for (const auto& item : *list) {
    const std::string* package_name = item.GetIfString();
    if (!package_name) {
      VLOG(1) << "Non-string value found in list. Ignoring all results.";
      allowed_packages_.clear();
      return false;
    }
    allowed_packages_.emplace(*package_name);
  }
  DVLOG(1) << "Added " << allowed_packages_.size() << " entries";
  return true;
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
  DVLOG(1) << "Adding new entry in task ID map: {" << task_id << ", "
           << package_name << "}";
  task_id_map_.emplace(task_id, package_name);

  // Add or update the respective entry in PackageNameMap
  PackageNameMap::iterator entry = package_name_map_.find(package_name);
  if (entry == package_name_map_.end()) {
    // Add new entry
    DVLOG(1) << "Adding new entry to package name map:" << package_name;
    package_name_map_.emplace(
        package_name, std::make_pair(1, base::Time::NowFromSystemTime()));
  } else {
    // Update the count for the existing entry.
    entry->second.first++;
    DVLOG(1) << "Updating package name map: " << package_name;
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
  DVLOG(1) << "Removing entry from task ID map: {" << task_id_iterator->first
           << ", " << task_id_iterator->second << "}";
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
    DVLOG(1) << "Removing from package name map:  " << package_name;
    package_name_map_.erase(package_name_iterator);

    // Trigger ArcGames survey if it's been active for at least
    // |elapsed_time_survey_trigger_|.
    base::TimeDelta elapsed_time =
        base::Time::NowFromSystemTime() - on_task_create_timestamp;
    if (elapsed_time >= elapsed_time_survey_trigger_) {
      DVLOG(1) << "Elapsed time is more than " << elapsed_time_survey_trigger_
               << ": " << elapsed_time.InMinutes();
      if (ash::HatsNotificationController::ShouldShowSurveyToProfile(
              profile_, ash::kHatsArcGamesSurvey)) {
        const base::flat_map<std::string, std::string> product_specific_data = {
            {kKeyMostRecentAndroidGame, package_name}};
        hats_notification_controller_ =
            base::MakeRefCounted<ash::HatsNotificationController>(
                profile_, ash::kHatsArcGamesSurvey, product_specific_data);
      }
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

// static
void ArcSurveyService::EnsureFactoryBuilt() {
  ArcSurveyServiceFactory::GetInstance();
}

}  // namespace arc
