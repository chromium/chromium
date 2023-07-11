// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

using SimulateRightClickModifier = ui::mojom::SimulateRightClickModifier;

const char kNotifierId[] = "input_device_settings_controller";
const char kAltRightClickRewriteNotificationId[] =
    "alt_right_click_rewrite_blocked_by_setting";
const char kSearchRightClickRewriteNotificationId[] =
    "search_right_click_rewrite_blocked_by_setting";
const char kRightClickRewriteDisabledNotificationId[] =
    "right_click_rewrite_disabled_by_setting";

bool IsRightClickRewriteDisabled(SimulateRightClickModifier active_modifier) {
  return active_modifier == SimulateRightClickModifier::kNone;
}

std::u16string GetRightClickRewriteNotificationMessage(
    SimulateRightClickModifier blocked_modifier,
    SimulateRightClickModifier active_modifier) {
  if (IsRightClickRewriteDisabled(active_modifier)) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_RIGHT_CLICK_DISABLED);
  }

  switch (blocked_modifier) {
    case SimulateRightClickModifier::kAlt:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_ALT_RIGHT_CLICK);
    case SimulateRightClickModifier::kSearch:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_LAUNCHER_RIGHT_CLICK);
    case SimulateRightClickModifier::kNone:
      NOTREACHED_NORETURN();
  }
}

std::string GetRightClickNotificationId(
    SimulateRightClickModifier blocked_modifier,
    SimulateRightClickModifier active_modifier) {
  if (IsRightClickRewriteDisabled(active_modifier)) {
    return kRightClickRewriteDisabledNotificationId;
  }
  switch (blocked_modifier) {
    case SimulateRightClickModifier::kAlt:
      return kAltRightClickRewriteNotificationId;
    case SimulateRightClickModifier::kSearch:
      return kSearchRightClickRewriteNotificationId;
    case SimulateRightClickModifier::kNone:
      NOTREACHED_NORETURN();
  }
}

}  // namespace

InputDeviceSettingsNotificationController::
    InputDeviceSettingsNotificationController(
        message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  CHECK(message_center_);
}

InputDeviceSettingsNotificationController::
    ~InputDeviceSettingsNotificationController() = default;

void InputDeviceSettingsNotificationController::
    NotifyRightClickRewriteBlockedBySetting(
        SimulateRightClickModifier blocked_modifier,
        SimulateRightClickModifier active_modifier) {
  CHECK_NE(blocked_modifier, SimulateRightClickModifier::kNone);
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GetRightClickNotificationId(blocked_modifier, active_modifier),
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE),
      GetRightClickRewriteNotificationMessage(blocked_modifier,
                                              active_modifier),
      std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          NotificationCatalogName::kEventRewriterDeprecation),
      message_center::RichNotificationData(), nullptr,
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
