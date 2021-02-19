// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

const char kNotifierId[] = "event_rewriter_deprecation_controller";
const char kNotificationIdPrefix[] = "alt_event_rewrite_deprecation.";

}  // namespace

DeprecationNotificationController::DeprecationNotificationController(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  DCHECK(message_center_);
}

DeprecationNotificationController::~DeprecationNotificationController() =
    default;

bool DeprecationNotificationController::NotifyDeprecatedRightClickRewrite() {
  if (!show_right_click_notification_) {
    return false;
  }

  const std::string id = std::string(kNotificationIdPrefix) + "right_click";
  ShowNotificationFromIdWithLauncherKey(id,
                                        IDS_ASH_SHORTCUT_DEPRECATION_ALT_CLICK);

  // Don't show the notification again.
  show_right_click_notification_ = false;
  return true;
}

void DeprecationNotificationController::ShowNotificationFromIdWithLauncherKey(
    const std::string& id,
    int message_id) {
  const int launcher_key_name_id = ui::DeviceUsesKeyboardLayout2()
                                       ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
                                       : IDS_ASH_SHORTCUT_MODIFIER_SEARCH;
  const base::string16 launcher_key_name =
      l10n_util::GetStringUTF16(launcher_key_name_id);
  const base::string16 message_body =
      l10n_util::GetStringFUTF16(message_id, launcher_key_name);

  ShowNotification(id, message_body);
}

void DeprecationNotificationController::ShowNotification(
    const std::string& id,
    const base::string16& message_body) {
  auto on_click_handler =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {
            if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
              Shell::Get()->shell_delegate()->OpenKeyboardShortcutHelpPage();
          }));

  auto notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE), message_body,
      base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      message_center::RichNotificationData(), std::move(on_click_handler),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
