// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_

#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

// Used to show a Happiness Tracking Survey when the audio server sends a
// trigger event.
class AudioSurveyHandler : public CrasAudioHandler::AudioObserver {
 public:
  AudioSurveyHandler();

  AudioSurveyHandler(const AudioSurveyHandler&) = delete;
  AudioSurveyHandler& operator=(const AudioSurveyHandler&) = delete;

  ~AudioSurveyHandler() override;

  // CrasAudioHandler::AudioObserver
  void OnSurveyTriggered(
      const CrasAudioHandler::AudioSurveyData& survey_specific_data) override;

 private:
  scoped_refptr<HatsNotificationController> hats_notification_controller_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
