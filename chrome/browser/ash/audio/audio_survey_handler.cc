// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/audio_survey_handler.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

AudioSurveyHandler::AudioSurveyHandler() {
  if (!base::FeatureList::IsEnabled(kHatsAudioSurvey.feature)) {
    VLOG(1) << "Audio survey feature is not enabled";
    return;
  }

  CrasAudioHandler::Get()->AddAudioObserver(this);
}

AudioSurveyHandler::~AudioSurveyHandler() {
  if (CrasAudioHandler::Get()) {
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
  }
}

void AudioSurveyHandler::OnSurveyTriggered(
    const CrasAudioHandler::AudioSurveyData& survey_specific_data) {
  Profile* profile = ProfileManager::GetActiveUserProfile();

  if (has_triggered_) {
    return;
  }

  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsAudioSurvey)) {
    return;
  }

  has_triggered_ = true;
  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &AudioSurveyHandler::OnHardwareInfoFetched,
      weak_ptr_factory_.GetWeakPtr(), std::move(survey_specific_data)));
}

void AudioSurveyHandler::OnHardwareInfoFetched(
    const CrasAudioHandler::AudioSurveyData& audio_specific_data,
    base::SysInfo::HardwareInfo hardware_info) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  base::flat_map<std::string, std::string> survey_data = {
      {"Board", base::SysInfo::GetLsbReleaseBoard()},
      {"Model", hardware_info.model}};
  survey_data.insert(audio_specific_data.begin(), audio_specific_data.end());

  hats_notification_controller_ =
      base::MakeRefCounted<HatsNotificationController>(
          profile, kHatsAudioSurvey, survey_data);
}

}  // namespace ash
