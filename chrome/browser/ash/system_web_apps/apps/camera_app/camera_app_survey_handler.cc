// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_app_survey_handler.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

using ash::HatsNotificationController;
using ash::kHatsCameraAppSurvey;

CameraAppSurveyHandler::CameraAppSurveyHandler() = default;

CameraAppSurveyHandler::~CameraAppSurveyHandler() = default;

// static
CameraAppSurveyHandler* CameraAppSurveyHandler::GetInstance() {
  return base::Singleton<CameraAppSurveyHandler>::get();
}

void CameraAppSurveyHandler::MaybeTriggerSurvey(const AccountId& account_id) {
  if (has_triggered_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(kHatsCameraAppSurvey.feature)) {
    VLOG(1) << "Camera App survey feature is not enabled";
    return;
  }

  user_manager::User* const user =
      user_manager::UserManager::Get()->FindUserAndModify(account_id);
  if (!user) {
    return;
  }

  if (!HatsNotificationController::ShouldShowSurveyToUser(
          *user, kHatsCameraAppSurvey)) {
    VLOG(1) << "Camera App survey should not show";
    return;
  }

  has_triggered_ = true;
  base::SysInfo::GetHardwareInfo(
      base::BindOnce(&CameraAppSurveyHandler::OnHardwareInfoFetched,
                     weak_ptr_factory_.GetWeakPtr(), account_id));
}

void CameraAppSurveyHandler::OnHardwareInfoFetched(
    const AccountId& account_id,
    base::SysInfo::HardwareInfo info) {
  user_manager::User* const user =
      user_manager::UserManager::Get()->FindUserAndModify(account_id);
  if (!user) {
    return;
  }

  base::flat_map<std::string, std::string> survey_specific_data = {
      {"board", base::SysInfo::GetLsbReleaseBoard()}, {"model", info.model}};
  // TODO(b/237737023): Add CUJ information if we want to collect more signals.

  hats_notification_controller_ =
      base::MakeRefCounted<ash::HatsNotificationController>(
          user, kHatsCameraAppSurvey, survey_specific_data);
}
