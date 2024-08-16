// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/screen_security_controller.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

// It is possible that we are capturing and sharing screen at the same time, so
// we cannot share the notification IDs for capturing and sharing.
const char kScreenAccessNotificationId[] = "chrome://screen/access";
const char kRemotingScreenShareNotificationId[] =
    "chrome://screen/remoting-share";

ScreenSecurityController::ScreenSecurityController() {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenSecurityObserver(this);
}

ScreenSecurityController::~ScreenSecurityController() {
  Shell::Get()->system_tray_notifier()->RemoveScreenSecurityObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void ScreenSecurityController::StopAllSessions(bool is_screen_access) {
  message_center::MessageCenter::Get()->RemoveNotification(
      is_screen_access ? kScreenAccessNotificationId
                       : kRemotingScreenShareNotificationId,
      /*by_user=*/false);

  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, is_screen_access ? screen_access_stop_callbacks_
                                        : remoting_share_stop_callbacks_);
  for (base::OnceClosure& callback : callbacks) {
    if (callback) {
      std::move(callback).Run();
    }
  }

  change_source_callback_.Reset();
}

void ScreenSecurityController::CreateNotification(
    const std::u16string& message,
    bool is_screen_access_notification) {
  if (features::IsVideoConferenceEnabled()) {
    // Don't send screen share notifications, because the VideoConferenceTray
    // serves as the notifier for screen share. As for screen capture, continue
    // to show these notifications for now, although they may end up in the
    // `VideoConferenceTray` as well. See b/269486186 for details.
    DCHECK(is_screen_access_notification);
  }

  message_center::RichNotificationData data;
  data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_ACCESS_STOP));
  // Only add "Change source" button when there is one session, since there
  // isn't a good UI to distinguish between the different sessions.
  if (is_screen_access_notification && change_source_callback_ &&
      screen_access_stop_callbacks_.size() == 1) {
    data.buttons.emplace_back(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_CHANGE_SOURCE));
  }

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](base::WeakPtr<ScreenSecurityController> controller,
                 bool is_screen_access_notification,
                 std::optional<int> button_index) {
                if (!button_index)
                  return;

                if (*button_index == 0) {
                  controller->StopAllSessions(
                      /*is_screen_access=*/is_screen_access_notification);
                } else if (*button_index == 1) {
                  controller->ChangeSource();
                  if (is_screen_access_notification) {
                    base::RecordAction(base::UserMetricsAction(
                        "StatusArea_ScreenCapture_Change_Source"));
                  }
                } else {
                  NOTREACHED();
                }
              },
              weak_ptr_factory_.GetWeakPtr(), is_screen_access_notification));

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      is_screen_access_notification ? kScreenAccessNotificationId
                                    : kRemotingScreenShareNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE),
      message, std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kPrivacyIndicatorsNotifierId,
                                 NotificationCatalogName::kPrivacyIndicators),
      data, std::move(delegate),
      /*small_image=*/kPrivacyIndicatorsScreenShareIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_pinned(true);
  notification->set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);
  notification->set_parent_vector_small_image(kPrivacyIndicatorsIcon);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ScreenSecurityController::ChangeSource() {
  if (change_source_callback_ && screen_access_stop_callbacks_.size() == 1) {
    change_source_callback_.Run();
  }
}

void ScreenSecurityController::OnScreenAccessStart(
    base::OnceClosure stop_callback,
    const base::RepeatingClosure& source_callback,
    const std::u16string& access_app_name) {
  screen_access_stop_callbacks_.emplace_back(std::move(stop_callback));
  change_source_callback_ = source_callback;

  // Don't send screen access notifications, because the VideoConferenceTray
  // serves as the notifier for this.
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  // We do not want to show the screen capture notification and the chromecast
  // casting tray notification at the same time.
  //
  // This suppression technique is currently dependent on the order
  // that OnScreenCaptureStart and OnCastingSessionStartedOrStopped
  // get invoked. OnCastingSessionStartedOrStopped currently gets
  // called first.
  if (is_casting_)
    return;

  CreateNotification(access_app_name, /*is_screen_access_notification=*/true);
  UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/true);
}

void ScreenSecurityController::OnScreenAccessStop() {
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  StopAllSessions(/*is_screen_access=*/true);
  UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/false);
}

void ScreenSecurityController::OnRemotingScreenShareStart(
    base::OnceClosure stop_callback) {
  remoting_share_stop_callbacks_.emplace_back(std::move(stop_callback));

  // Don't send screen share notifications, because the VideoConferenceTray
  // serves as the notifier for screen share.
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  CreateNotification(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED),
      /*is_screen_access_notification=*/false);
  UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/true);
}

void ScreenSecurityController::OnRemotingScreenShareStop() {
  if (features::IsVideoConferenceEnabled()) {
    return;
  }

  StopAllSessions(/*is_screen_access=*/false);
  UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/false);
}

void ScreenSecurityController::OnCastingSessionStartedOrStopped(bool started) {
  is_casting_ = started;
}

}  // namespace ash
