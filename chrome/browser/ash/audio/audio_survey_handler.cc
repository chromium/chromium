// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/audio_survey_handler.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// The user `account_id` names, who is signed in whenever this is reached.
user_manager::User& FindUser(const AccountId& account_id) {
  return CHECK_DEREF(
      user_manager::UserManager::Get()->FindUserAndModify(account_id));
}

}  // namespace

const HatsConfig& GetHatsConfig(CrasAudioHandler::SurveyType type) {
  switch (type) {
    case CrasAudioHandler::SurveyType::kGeneral:
      return kHatsAudioSurvey;
    case CrasAudioHandler::SurveyType::kBluetooth:
      return kHatsBluetoothAudioSurvey;
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

  bool ShouldShowSurvey(CrasAudioHandler::SurveyType type,
                        const AccountId& account_id) const override {
    return !hats_notification_controllers_.contains(type) &&
           HatsNotificationController::ShouldShowSurveyToUser(
               FindUser(account_id), GetHatsConfig(type));
  }

  void ShowSurvey(CrasAudioHandler::SurveyType type,
                  const CrasAudioHandler::AudioSurveyData& data,
                  const AccountId& account_id) override {
    base::SysInfo::GetHardwareInfo(
        base::BindOnce(&AudioSurveyHandlerDelegate::OnHardwareInfoFetched,
                       weak_ptr_factory_.GetWeakPtr(), type, data, account_id));
  }

 private:
  void OnHardwareInfoFetched(
      CrasAudioHandler::SurveyType type,
      const CrasAudioHandler::AudioSurveyData& audio_specific_data,
      const AccountId& account_id,
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
      !base::FeatureList::IsEnabled(kHatsBluetoothAudioSurvey.feature)) {
    VLOG(1) << "Audio survey feature is not enabled";
    return;
  }

  audio_observer_.Observe(delegate_.get());
}

AudioSurveyHandler::~AudioSurveyHandler() = default;

void AudioSurveyHandler::OnSurveyTriggered(
    const CrasAudioHandler::AudioSurvey& survey) {
  // The audio server decides when to trigger, so this is the first point that
  // can say who the survey is for. There is nobody to ask before log in.
  const session_manager::Session* const session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!session) {
    return;
  }

  auto type = survey.type();
  if (!delegate_->ShouldShowSurvey(type, session->account_id())) {
    return;
  }

  delegate_->ShowSurvey(type, survey.data(), session->account_id());
}

void AudioSurveyHandlerDelegate::OnHardwareInfoFetched(
    CrasAudioHandler::SurveyType type,
    const CrasAudioHandler::AudioSurveyData& audio_specific_data,
    const AccountId& account_id,
    base::SysInfo::HardwareInfo hardware_info) {
  base::flat_map<std::string, std::string> survey_data = {
      {"Board", base::SysInfo::GetLsbReleaseBoard()},
      {"Model", hardware_info.model}};
  survey_data.insert(audio_specific_data.begin(), audio_specific_data.end());

  hats_notification_controllers_[type] =
      base::MakeRefCounted<HatsNotificationController>(
          &FindUser(account_id), GetHatsConfig(type), survey_data);
}

}  // namespace ash
