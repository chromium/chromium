// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/audio_survey_handler.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

const HatsConfig& GetHatsConfig(CrasAudioHandler::SurveyType type) {
  switch (type) {
    case CrasAudioHandler::SurveyType::kGeneral:
      return kHatsAudioSurvey;
    case CrasAudioHandler::SurveyType::kBluetooth:
      return kHatsBluetoothAudioSurvey;
    case CrasAudioHandler::SurveyType::kOutputProc:
      return kHatsAudioOutputProcSurvey;
  }
}

class AudioSurveyHandlerDelegate : public AudioSurveyHandler::Delegate {
 public:
  AudioSurveyHandlerDelegate() = default;
  ~AudioSurveyHandlerDelegate() override = default;
  AudioSurveyHandlerDelegate(const AudioSurveyHandlerDelegate&) = delete;
  AudioSurveyHandlerDelegate& operator=(const AudioSurveyHandlerDelegate&) =
      delete;

  void AddAudioObserver(CrasAudioHandler::AudioObserver* observer) override {
    CrasAudioHandler::Get()->AddAudioObserver(observer);
  }

  void RemoveAudioObserver(CrasAudioHandler::AudioObserver* observer) override {
    CrasAudioHandler::Get()->RemoveAudioObserver(observer);
  }

  bool ShouldShowSurvey(CrasAudioHandler::SurveyType type) const override {
    return !hats_notification_controllers_.contains(type) &&
           HatsNotificationController::ShouldShowSurveyToProfile(
               ProfileManager::GetActiveUserProfile(), GetHatsConfig(type));
  }

  void ShowSurvey(CrasAudioHandler::SurveyType type,
                  const CrasAudioHandler::AudioSurveyData& data) override {
    base::SysInfo::GetHardwareInfo(
        base::BindOnce(&AudioSurveyHandlerDelegate::OnHardwareInfoFetched,
                       weak_ptr_factory_.GetWeakPtr(), type, data));
  }

 private:
  void OnHardwareInfoFetched(
      CrasAudioHandler::SurveyType type,
      const CrasAudioHandler::AudioSurveyData& audio_specific_data,
      base::SysInfo::HardwareInfo hardware_info);

  base::flat_map<CrasAudioHandler::SurveyType,
                 scoped_refptr<HatsNotificationController>>
      hats_notification_controllers_;
  base::WeakPtrFactory<AudioSurveyHandlerDelegate> weak_ptr_factory_{this};
};

AudioSurveyHandler::AudioSurveyHandler()
    : AudioSurveyHandler(std::make_unique<AudioSurveyHandlerDelegate>()) {}

AudioSurveyHandler::AudioSurveyHandler(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  if (!base::FeatureList::IsEnabled(kHatsAudioSurvey.feature) &&
      !base::FeatureList::IsEnabled(kHatsAudioOutputProcSurvey.feature) &&
      !base::FeatureList::IsEnabled(kHatsBluetoothAudioSurvey.feature)) {
    VLOG(1) << "Audio survey feature is not enabled";
    return;
  }

  audio_observer_.Observe(delegate_.get());
}

AudioSurveyHandler::~AudioSurveyHandler() = default;

void AudioSurveyHandler::OnSurveyTriggered(
    const CrasAudioHandler::AudioSurvey& survey) {
  auto type = survey.type();
  if (!delegate_->ShouldShowSurvey(type)) {
    return;
  }

  delegate_->ShowSurvey(type, survey.data());
}

void AudioSurveyHandlerDelegate::OnHardwareInfoFetched(
    CrasAudioHandler::SurveyType type,
    const CrasAudioHandler::AudioSurveyData& audio_specific_data,
    base::SysInfo::HardwareInfo hardware_info) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  base::flat_map<std::string, std::string> survey_data = {
      {"Board", base::SysInfo::GetLsbReleaseBoard()},
      {"Model", hardware_info.model}};
  survey_data.insert(audio_specific_data.begin(), audio_specific_data.end());

  hats_notification_controllers_[type] =
      base::MakeRefCounted<HatsNotificationController>(
          profile, GetHatsConfig(type), survey_data);
}

}  // namespace ash
