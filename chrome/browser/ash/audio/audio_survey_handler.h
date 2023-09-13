// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_

#include "base/system/sys_info.h"
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

  void OnHardwareInfoFetched(
      const CrasAudioHandler::AudioSurveyData& audio_specific_data,
      base::SysInfo::HardwareInfo hardware_info);

 private:
  scoped_refptr<HatsNotificationController> hats_notification_controller_;

  bool has_triggered_ = false;

  base::WeakPtrFactory<AudioSurveyHandler> weak_ptr_factory_{this};
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
