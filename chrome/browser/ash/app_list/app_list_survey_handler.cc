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

  std::string experimental_group = "";

  switch (experimental_arm) {
    case ash::AppsCollectionsController::ExperimentalArm::kDefaultValue:
    case ash::AppsCollectionsController::ExperimentalArm::kControl:
      return;
    case ash::AppsCollectionsController::ExperimentalArm::kEnabled:
      experimental_group = "enabled";
      break;
    case ash::AppsCollectionsController::ExperimentalArm::kCounterfactual:
      experimental_group = "counterfactual";
      break;
    case ash::AppsCollectionsController::ExperimentalArm::kModifiedOrder:
      experimental_group = "modified_order";
      break;
  }

  const base::flat_map<std::string, std::string> product_specific_data = {
      {"experiment_group", experimental_group}};

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
