// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"

#include <cstdint>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/microphone_mute_notification_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

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
  mic_mute_on_ = mute_on;
  mic_muted_by_mute_switch_ =
      CrasAudioHandler::Get()->input_muted_by_microphone_mute_switch();

  // Show the notification with low priority when the mute state changes, so the
  // notification gets shown silently. This prevents the notification from
  // interfering with the mute state change toast that gets shown on mute state
  // change.
  MaybeShowNotification(
      current_notification_priority_.value_or(message_center::LOW_PRIORITY),
      /*recreate=*/false);
}

void MicrophoneMuteNotificationController::
    OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) {
  if (mic_muted_by_mute_switch_ == muted)
    return;

  mic_muted_by_mute_switch_ = muted;
  // Show the notification with low priority when the mute state changes, so the
  // notification gets shown silently. This prevents the notification from
  // interfering with the mute state change toast that gets shown on mute state
  // change.
  MaybeShowNotification(
      current_notification_priority_.value_or(message_center::LOW_PRIORITY),
      /*recreate=*/false);
}

void MicrophoneMuteNotificationController::
    OnNumberOfInputStreamsWithPermissionChanged() {
  // Catches the case where a mic-using app is launched while the mic is muted.
  const int input_stream_count = CountActiveInputStreams();
  const bool stream_count_decreased = input_stream_count < input_stream_count_;
  input_stream_count_ = input_stream_count;

  if (!input_stream_count_) {
    RemoveMicrophoneMuteNotification();
    return;
  }

  MaybeShowNotification(message_center::DEFAULT_PRIORITY,
                        /*recreate=*/!stream_count_decreased);
}

void MicrophoneMuteNotificationController::MaybeShowNotification(
    message_center::NotificationPriority priority,
    bool recreate) {
  if (mic_mute_on_) {
    absl::optional<std::u16string> app_name =
        MicrophoneMuteNotificationDelegate::Get()->GetAppAccessingMicrophone();
    if (app_name.has_value() || input_stream_count_) {
      if (recreate)
        RemoveMicrophoneMuteNotification();

      std::unique_ptr<message_center::Notification> notification =
          GenerateMicrophoneMuteNotification(app_name, priority);
      message_center::MessageCenter::Get()->AddNotification(
          std::move(notification));
      return;
    }
  }

  RemoveMicrophoneMuteNotification();
}

std::unique_ptr<message_center::Notification>
MicrophoneMuteNotificationController::GenerateMicrophoneMuteNotification(
    const absl::optional<std::u16string>& app_name,
    message_center::NotificationPriority priority) {
  message_center::RichNotificationData notification_data;
  notification_data.priority = priority;
  current_notification_priority_ = priority;
  notification_data.pinned = true;

  scoped_refptr<message_center::NotificationDelegate> delegate;
  // Don't show a button to unmute device if the microphone was muted by a HW
  // mute switch, as in that case unmute action would not work.
  if (!mic_muted_by_mute_switch_) {
    notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
        IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON));

    delegate =
        base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
            base::BindRepeating([](absl::optional<int> button_index) {
              // Click on the notification body is no-op.
              if (!button_index)
                return;
              CrasAudioHandler::Get()->SetInputMute(false);
            }));
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          GetNotificationTitle(app_name), GetNotificationMessage(),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId,
              NotificationCatalogName::kMicrophoneMute),
          notification_data, delegate, vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  return notification;
}

std::u16string MicrophoneMuteNotificationController::GetNotificationMessage()
    const {
  return mic_muted_by_mute_switch_
             ? l10n_util::GetStringUTF16(
                   IDS_MICROPHONE_MUTE_SWITCH_ON_NOTIFICATION_MESSAGE)
             : l10n_util::GetStringUTF16(
                   IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE);
}

std::u16string MicrophoneMuteNotificationController::GetNotificationTitle(
    const absl::optional<std::u16string>& app_name) const {
  return !app_name.value_or(u"").empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_MICROPHONE_MUTED_NOTIFICATION_TITLE_WITH_APP_NAME,
                   app_name.value())
             : l10n_util::GetStringUTF16(
                   IDS_MICROPHONE_MUTED_NOTIFICATION_TITLE);
}

void MicrophoneMuteNotificationController::RemoveMicrophoneMuteNotification() {
  current_notification_priority_.reset();
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);
}

int MicrophoneMuteNotificationController::CountActiveInputStreams() {
  int num_active_streams = 0;
  base::flat_map<CrasAudioHandler::ClientType, uint32_t> input_streams =
      CrasAudioHandler::Get()->GetNumberOfInputStreamsWithPermission();
  for (auto& client_type_info : input_streams)
    num_active_streams += client_type_info.second;

  return num_active_streams;
}

}  // namespace ash
