// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_app_survey_handler.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/profiles/profile_manager.h"

using ash::HatsNotificationController;
using ash::kHatsCameraAppSurvey;

CameraAppSurveyHandler::CameraAppSurveyHandler() = default;

CameraAppSurveyHandler::~CameraAppSurveyHandler() = default;

// static
CameraAppSurveyHandler* CameraAppSurveyHandler::GetInstance() {
  return base::Singleton<CameraAppSurveyHandler>::get();
}

void CameraAppSurveyHandler::MaybeTriggerSurvey() {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  if (has_triggered_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kHatsCameraAppSurvey.feature)) {
    VLOG(1) << "Camera App survey feature is not enabled";
    return;
  }

  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsCameraAppSurvey)) {
    VLOG(1) << "Camera App survey should not show";
    return;
  }

  has_triggered_ = true;
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&CameraAppSurveyHandler::OnHardwareInfoFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CameraAppSurveyHandler::OnHardwareInfoFetched(
    base::SysInfo::HardwareInfo info) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  base::flat_map<std::string, std::string> survey_specific_data = {
      {"board", base::SysInfo::GetLsbReleaseBoard()}, {"model", info.model}};
  // TODO(b/237737023): Add CUJ information if we want to collect more signals.

  hats_notification_controller_ =
      base::MakeRefCounted<ash::HatsNotificationController>(
          profile, kHatsCameraAppSurvey, survey_specific_data);
}
