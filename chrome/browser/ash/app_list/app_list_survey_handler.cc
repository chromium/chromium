// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_survey_handler.h"

#include <string.h>

#include "ash/app_list/apps_collections_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"

namespace app_list {

AppListSurveyHandler::AppListSurveyHandler(Profile* profile)
    : profile_(profile) {
  if (profile) {
    profile_observation_.Observe(profile);
  }
}

AppListSurveyHandler::~AppListSurveyHandler() = default;

void AppListSurveyHandler::MaybeTriggerSurvey() {
  if (!profile_ || hats_notification_controller_) {
    return;
  }

  ash::AppsCollectionsController::ExperimentalArm experimental_arm =
      ash::AppsCollectionsController::Get()->GetUserExperimentalArm();

  // This survey only cares for AppsCollections experiment group.
  if (experimental_arm ==
      ash::AppsCollectionsController::ExperimentalArm::kControl) {
    return;
  }

  const base::flat_map<std::string, std::string> product_specific_data = {
      {"experiment_group",
       experimental_arm ==
               ash::AppsCollectionsController::ExperimentalArm::kEnabled
           ? "enabled"
           : "counterfactual"}};

  if (ash::HatsNotificationController::ShouldShowSurveyToProfile(
          profile_, ash::kHatsLauncherAppsFindingSurvey)) {
    hats_notification_controller_ =
        base::MakeRefCounted<ash::HatsNotificationController>(
            profile_, ash::kHatsLauncherAppsFindingSurvey);
  } else if (ash::HatsNotificationController::ShouldShowSurveyToProfile(
                 profile_, ash::kHatsLauncherAppsNeedingSurvey)) {
    hats_notification_controller_ =
        base::MakeRefCounted<ash::HatsNotificationController>(
            profile_, ash::kHatsLauncherAppsNeedingSurvey);
  }
}

void AppListSurveyHandler::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

ash::HatsNotificationController*
AppListSurveyHandler::GetHatsNotificationControllerForTesting() const {
  return hats_notification_controller_.get();
}

}  // namespace app_list
