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

  // Do not run more than one HATS survey.
  if (hats_notification_controller_)
    return;

  if (HatsNotificationController::ShouldShowSurveyToProfile(profile,
                                                            kHatsAudioSurvey)) {
    hats_notification_controller_ = new HatsNotificationController(
        profile, kHatsAudioSurvey, survey_specific_data);
  }
}

}  // namespace ash
