// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ash {

// Used to show a Happiness Tracking Survey when the audio server sends a
// trigger event.
class AudioSurveyHandler : public CrasAudioHandler::AudioObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Adds the survey handler as an observer of AudioObserver.
    virtual void AddAudioObserver(
        CrasAudioHandler::AudioObserver* observer) = 0;

    // Removes the survey handler as an observer of AudioObserver.
    virtual void RemoveAudioObserver(
        CrasAudioHandler::AudioObserver* observer) = 0;

    // Checks whether the device met all conditions to participate the survey.
    virtual bool ShouldShowSurvey(CrasAudioHandler::SurveyType type) const = 0;

    // Triggers the survey invitation notification, in which when open will
    // show the audio survey.
    virtual void ShowSurvey(CrasAudioHandler::SurveyType type,
                            const CrasAudioHandler::AudioSurveyData& data) = 0;
  };

  AudioSurveyHandler();
  // This is used for testing
  explicit AudioSurveyHandler(std::unique_ptr<Delegate> delegate);
  AudioSurveyHandler(const AudioSurveyHandler&) = delete;
  AudioSurveyHandler& operator=(const AudioSurveyHandler&) = delete;
  ~AudioSurveyHandler() override;

  // CrasAudioHandler::AudioObserver
  void OnSurveyTriggered(const CrasAudioHandler::AudioSurvey& survey) override;

 private:
  const std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<Delegate, CrasAudioHandler::AudioObserver>
      audio_observer_{this};
  base::WeakPtrFactory<AudioSurveyHandler> weak_ptr_factory_{this};
};

}  // namespace ash
namespace base {

template <>
struct ScopedObservationTraits<ash::AudioSurveyHandler::Delegate,
                               ash::CrasAudioHandler::AudioObserver> {
  static void AddObserver(ash::AudioSurveyHandler::Delegate* source,
                          ash::CrasAudioHandler::AudioObserver* observer) {
    source->AddAudioObserver(observer);
  }
  static void RemoveObserver(ash::AudioSurveyHandler::Delegate* source,
                             ash::CrasAudioHandler::AudioObserver* observer) {
    source->RemoveAudioObserver(observer);
  }
};

}  // namespace base
#endif  // CHROME_BROWSER_ASH_AUDIO_AUDIO_SURVEY_HANDLER_H_
