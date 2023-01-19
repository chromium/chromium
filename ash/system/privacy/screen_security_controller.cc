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
const char kScreenCaptureNotificationId[] = "chrome://screen/capture";
const char kScreenShareNotificationId[] = "chrome://screen/share";
const char kNotifierScreenCapture[] = "ash.screen-capture";
const char kNotifierScreenShare[] = "ash.screen-share";

ScreenSecurityController::ScreenSecurityController() {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenCaptureObserver(this);
  Shell::Get()->system_tray_notifier()->AddScreenShareObserver(this);
}

ScreenSecurityController::~ScreenSecurityController() {
  Shell::Get()->system_tray_notifier()->RemoveScreenShareObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveScreenCaptureObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void ScreenSecurityController::CreateNotification(const std::u16string& message,
                                                  bool is_capture) {
  message_center::RichNotificationData data;
  data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
      is_capture ? IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_STOP
                 : IDS_ASH_STATUS_TRAY_SCREEN_SHARE_STOP)));
  // Only add "Change source" button when there is one session, since there
  // isn't a good UI to distinguish between the different sessions.
  if (is_capture && change_source_callback_ &&
      capture_stop_callbacks_.size() == 1) {
    data.buttons.push_back(message_center::ButtonInfo(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_CAPTURE_CHANGE_SOURCE)));
  }

  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](base::WeakPtr<ScreenSecurityController> controller,
                 bool is_capture, absl::optional<int> button_index) {
                if (!button_index)
                  return;

                if (*button_index == 0) {
                  controller->StopAllSessions(is_capture);
                } else if (*button_index == 1) {
                  controller->ChangeSource();
                  if (is_capture) {
                    base::RecordAction(base::UserMetricsAction(
                        "StatusArea_ScreenCapture_Change_Source"));
                  }
                } else {
                  NOTREACHED();
                }
              },
              weak_ptr_factory_.GetWeakPtr(), is_capture));

  // If the feature is enabled, the notification should have the style of
  // privacy indicators notification.
  auto* notifier_id =
      features::IsPrivacyIndicatorsEnabled()
          ? kPrivacyIndicatorsNotifierId
          : (is_capture ? kNotifierScreenCapture : kNotifierScreenShare);

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      is_capture ? kScreenCaptureNotificationId : kScreenShareNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SCREEN_SHARE_TITLE),
      message, std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, notifier_id,
          features::IsPrivacyIndicatorsEnabled()
              ? NotificationCatalogName::kPrivacyIndicators
              : NotificationCatalogName::kScreenSecurity),
      data, std::move(delegate),
      features::IsPrivacyIndicatorsEnabled() ? kPrivacyIndicatorsScreenShareIcon
                                             : kNotificationScreenshareIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  notification->set_pinned(true);

  if (features::IsPrivacyIndicatorsEnabled()) {
    notification->set_accent_color_id(ui::kColorAshPrivacyIndicatorsBackground);
    notification->set_parent_vector_small_image(kPrivacyIndicatorsIcon);
  }

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ScreenSecurityController::StopAllSessions(bool is_capture) {
  message_center::MessageCenter::Get()->RemoveNotification(
      is_capture ? kScreenCaptureNotificationId : kScreenShareNotificationId,
      false /* by_user */);

  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks,
            is_capture ? capture_stop_callbacks_ : share_stop_callbacks_);
  for (base::OnceClosure& callback : callbacks) {
    if (callback)
      std::move(callback).Run();
  }

  change_source_callback_.Reset();
}

void ScreenSecurityController::ChangeSource() {
  if (change_source_callback_ && capture_stop_callbacks_.size() == 1)
    change_source_callback_.Run();
}

void ScreenSecurityController::OnScreenCaptureStart(
    base::OnceClosure stop_callback,
    const base::RepeatingClosure& source_callback,
    const std::u16string& screen_capture_status) {
  capture_stop_callbacks_.emplace_back(std::move(stop_callback));
  change_source_callback_ = source_callback;

  // We do not want to show the screen capture notification and the chromecast
  // casting tray notification at the same time.
  //
  // This suppression technique is currently dependent on the order
  // that OnScreenCaptureStart and OnCastingSessionStartedOrStopped
  // get invoked. OnCastingSessionStartedOrStopped currently gets
  // called first.
  if (is_casting_)
    return;

  CreateNotification(screen_capture_status, true /* is_capture */);
}

void ScreenSecurityController::OnScreenCaptureStop() {
  StopAllSessions(true /* is_capture */);
}

void ScreenSecurityController::OnScreenShareStart(
    base::OnceClosure stop_callback,
    const std::u16string& helper_name) {
  share_stop_callbacks_.emplace_back(std::move(stop_callback));

  std::u16string help_label_text;
  if (!helper_name.empty()) {
    help_label_text = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED_NAME, helper_name);
  } else {
    help_label_text = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_SCREEN_SHARE_BEING_HELPED);
  }

  CreateNotification(help_label_text, false /* is_capture */);

  if (features::IsPrivacyIndicatorsEnabled())
    UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/true);
}

void ScreenSecurityController::OnScreenShareStop() {
  StopAllSessions(false /* is_capture */);

  if (features::IsPrivacyIndicatorsEnabled())
    UpdatePrivacyIndicatorsScreenShareStatus(/*is_screen_sharing=*/false);
}

void ScreenSecurityController::OnCastingSessionStartedOrStopped(bool started) {
  is_casting_ = started;
}

}  // namespace ash
