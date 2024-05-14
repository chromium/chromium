// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"

#include <string>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
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
  if (right_click_notification_shown_) {
    return false;
  }

  const std::string id = std::string(kNotificationIdPrefix) + "right_click";
  ShowNotificationFromIdWithLauncherKey(id,
                                        IDS_ASH_SHORTCUT_DEPRECATION_ALT_CLICK);

  // Don't show the notification again.
  right_click_notification_shown_ = true;
  return true;
}

bool DeprecationNotificationController::NotifyDeprecatedSixPackKeyRewrite(
    ui::KeyboardCode key_code) {
  if (!ShouldShowSixPackKeyDeprecationNotification(key_code)) {
    return false;
  }

  // The notification id is not user visible.
  const std::string id =
      std::string(kNotificationIdPrefix) + base::NumberToString(key_code);
  const int message_id = GetSixPackKeyDeprecationMessageId(key_code);
  ShowNotificationFromIdWithLauncherKey(id, message_id);

  // Keep track that the notification was shown to decide whether to show it
  // again in future.
  RecordSixPackKeyDeprecationNotificationShown(key_code);
  return true;
}

void DeprecationNotificationController::ResetStateForTesting() {
  shown_key_notifications_.clear();
  right_click_notification_shown_ = false;
}

void DeprecationNotificationController::ShowNotificationFromIdWithLauncherKey(
    const std::string& id,
    int message_id) {
  const int launcher_key_name_id =
      Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
          ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
          : IDS_ASH_SHORTCUT_MODIFIER_SEARCH;
  const std::u16string launcher_key_name =
      l10n_util::GetStringUTF16(launcher_key_name_id);
  const std::u16string message_body =
      l10n_util::GetStringFUTF16(message_id, launcher_key_name);

  ShowNotification(id, message_body);
}

void DeprecationNotificationController::ShowNotification(
    const std::string& id,
    const std::u16string& message_body) {
  auto on_click_handler =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {
            if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
              Shell::Get()->shell_delegate()->OpenKeyboardShortcutHelpPage();
          }));

  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, id,
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE), message_body,
      std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          NotificationCatalogName::kEventRewriterDeprecation),
      message_center::RichNotificationData(), std::move(on_click_handler),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

bool DeprecationNotificationController::
    ShouldShowSixPackKeyDeprecationNotification(ui::KeyboardCode key_code) {
  const auto* accelerator_controller = Shell::Get()->accelerator_controller();
  DCHECK(accelerator_controller);

  // Six pack key notification should not show if accelerators are being blocked
  // as the user does not expect these keys to be interpreted as a six pack key.
  return !accelerator_controller->ShouldPreventProcessingAccelerators() &&
         !shown_key_notifications_.contains(key_code);
}

void DeprecationNotificationController::
    RecordSixPackKeyDeprecationNotificationShown(ui::KeyboardCode key_code) {
  DCHECK(!shown_key_notifications_.contains(key_code));
  shown_key_notifications_.insert(key_code);
}

int DeprecationNotificationController::GetSixPackKeyDeprecationMessageId(
    ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_DELETE:
      return IDS_ASH_SHORTCUT_DEPRECATION_ALT_BASED_DELETE;
    case ui::VKEY_INSERT:
      return IDS_ASH_SHORTCUT_DEPRECATION_SEARCH_PERIOD_INSERT;
    case ui::VKEY_HOME:
      return IDS_ASH_SHORTCUT_DEPRECATION_ALT_BASED_HOME;
    case ui::VKEY_END:
      return IDS_ASH_SHORTCUT_DEPRECATION_ALT_BASED_END;
    case ui::VKEY_PRIOR:
      return IDS_ASH_SHORTCUT_DEPRECATION_ALT_BASED_PAGE_UP;
    case ui::VKEY_NEXT:
      return IDS_ASH_SHORTCUT_DEPRECATION_ALT_BASED_PAGE_DOWN;
    default:
      NOTREACHED_IN_MIGRATION();
      return -1;
  }
}

}  // namespace ash
