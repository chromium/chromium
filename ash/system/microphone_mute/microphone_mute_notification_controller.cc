// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"

#include <cstdint>
#include <string>

#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

// static
const char MicrophoneMuteNotificationController::kNotificationId[] =
    "ash://microphone_mute";

MicrophoneMuteNotificationController::MicrophoneMuteNotificationController() {
  audio_observation_.Observe(CrasAudioHandler::Get());
}

MicrophoneMuteNotificationController::~MicrophoneMuteNotificationController() =
    default;

void MicrophoneMuteNotificationController::OnInputMuteChanged(bool mute_on) {
  // Catches the case where the mic is muted while a mic-using app is running.
  mic_mute_on_ = mute_on;
  MaybeShowNotification();
}

void MicrophoneMuteNotificationController::MaybeShowNotification() {
  if (mic_mute_on_) {
    absl::optional<std::u16string> app_name =
        MicrophoneMuteNotificationDelegate::Get()->GetAppAccessingMicrophone();
    if (app_name.has_value() || HaveActiveInputStreams()) {
      GenerateMicrophoneMuteNotification(app_name);
      return;
    }
  }

  RemoveMicrophoneMuteNotification();
}

void MicrophoneMuteNotificationController::GenerateMicrophoneMuteNotification(
    const absl::optional<std::u16string>& app_name) {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          GetNotificationTitle(), GetNotificationMessage(app_name),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId),
          message_center::RichNotificationData(), nullptr,
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

std::u16string MicrophoneMuteNotificationController::GetNotificationMessage(
    const absl::optional<std::u16string>& app_name) const {
  return !app_name.value_or(u"").empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_MICROPHONE_MUTE_SWITCH_ON_NOTIFICATION_MESSAGE_WITH_APP_NAME,
                   app_name.value())
             : l10n_util::GetStringUTF16(
                   IDS_MICROPHONE_MUTE_SWITCH_ON_NOTIFICATION_MESSAGE);
}

std::u16string MicrophoneMuteNotificationController::GetNotificationTitle()
    const {
  return l10n_util::GetStringUTF16(
      IDS_MICROPHONE_MUTE_SWITCH_ON_NOTIFICATION_TITLE);
}

void MicrophoneMuteNotificationController::RemoveMicrophoneMuteNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);
}

bool MicrophoneMuteNotificationController::HaveActiveInputStreams() {
  base::flat_map<CrasAudioHandler::ClientType, uint32_t> input_streams =
      CrasAudioHandler::Get()->GetNumberOfInputStreamsWithPermission();
  for (auto& per_client_type_count : input_streams) {
    if (per_client_type_count.second > 0)
      return true;
  }
  return false;
}

void MicrophoneMuteNotificationController::
    OnNumberOfInputStreamsWithPermissionChanged() {
  // Catches the case where a mic-using app is launched while the mic is muted.
  MaybeShowNotification();
}

}  // namespace ash
