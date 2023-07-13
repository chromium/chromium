// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include <array>
#include <string>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

using SimulateRightClickModifier = ui::mojom::SimulateRightClickModifier;
using SixPackShortcutModifier = ui::mojom::SixPackShortcutModifier;

constexpr auto kSixPackKeyToPrefName =
    base::MakeFixedFlatMap<ui::KeyboardCode, const char*>({
        {ui::KeyboardCode::VKEY_DELETE,
         {prefs::kSixPackKeyDeleteNotificationsRemaining}},
        {ui::KeyboardCode::VKEY_HOME,
         {prefs::kSixPackKeyHomeNotificationsRemaining}},
        {ui::KeyboardCode::VKEY_PRIOR,
         {prefs::kSixPackKeyPageUpNotificationsRemaining}},
        {ui::KeyboardCode::VKEY_END,
         {prefs::kSixPackKeyEndNotificationsRemaining}},
        {ui::KeyboardCode::VKEY_NEXT,
         {prefs::kSixPackKeyPageDownNotificationsRemaining}},
        {ui::KeyboardCode::VKEY_INSERT,
         {prefs::kSixPackKeyInsertNotificationsRemaining}},
    });

const char kNotifierId[] = "input_device_settings_controller";
const char kAltRightClickRewriteNotificationId[] =
    "alt_right_click_rewrite_blocked_by_setting";
const char kSearchRightClickRewriteNotificationId[] =
    "search_right_click_rewrite_blocked_by_setting";
const char kRightClickRewriteDisabledNotificationId[] =
    "right_click_rewrite_disabled_by_setting";
const char kSixPackKeyDeleteRewriteNotificationId[] =
    "delete_six_pack_rewrite_blocked_by_setting";
const char kSixPackKeyInsertRewriteNotificationId[] =
    "insert_six_pack_rewrite_blocked_by_setting";
const char kSixPackKeyHomeRewriteNotificationId[] =
    "home_six_pack_rewrite_blocked_by_setting";
const char kSixPackKeyEndRewriteNotificationId[] =
    "end_six_pack_rewrite_blocked_by_setting";
const char kSixPackKeyPageUpRewriteNotificationId[] =
    "page_up_six_pack_rewrite_blocked_by_setting";
const char kSixPackKeyPageDownRewriteNotificationId[] =
    "page_down_six_pack_rewrite_blocked_by_setting";
const char kDelimiter[] = "_";

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

// We only display notifications for active user sessions (signed-in/guest with
// desktop ready). Also do not show notifications in signin or lock screen.
bool IsActiveUserSession() {
  const auto* session_controller = Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !session_controller->IsUserSessionBlocked();
}

// If the user has reached the settings page through the notification, do
// not show any more new notifications.
void StopShowingNotification(const char* pref_name) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      pref_name, 0);
}

bool ShouldShowSixPackKeyNotification() {
  const auto* accelerator_controller = Shell::Get()->accelerator_controller();
  CHECK(accelerator_controller);
  // Six pack key notification should not show if accelerators are being blocked
  // as the user does not expect these keys to be interpreted as a six pack key.
  return !accelerator_controller->ShouldPreventProcessingAccelerators() &&
         IsActiveUserSession();
}

std::u16string GetSixPackKeyName(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_DELETE:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_DELETE);
    case ui::VKEY_INSERT:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_INSERT);
    case ui::VKEY_HOME:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_HOME);
    case ui::VKEY_END:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_END);
    case ui::VKEY_PRIOR:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_PAGE_UP);
    case ui::VKEY_NEXT:
      return l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_SIX_PACK_KEY_PAGE_DOWN);
    default:
      NOTREACHED_NORETURN();
  }
}

std::string GetSixPackNotificationId(ui::KeyboardCode key_code, int device_id) {
  std::string notification_id;
  switch (key_code) {
    case ui::VKEY_DELETE:
      notification_id = kSixPackKeyDeleteRewriteNotificationId;
      break;
    case ui::VKEY_INSERT:
      notification_id = kSixPackKeyInsertRewriteNotificationId;
      break;
    case ui::VKEY_HOME:
      notification_id = kSixPackKeyHomeRewriteNotificationId;
      break;
    case ui::VKEY_END:
      notification_id = kSixPackKeyEndRewriteNotificationId;
      break;
    case ui::VKEY_PRIOR:
      notification_id = kSixPackKeyPageUpRewriteNotificationId;
      break;
    case ui::VKEY_NEXT:
      notification_id = kSixPackKeyPageDownRewriteNotificationId;
      break;
    default:
      NOTREACHED_NORETURN();
  }
  return notification_id + kDelimiter + base::NumberToString(device_id);
}

}  // namespace

InputDeviceSettingsNotificationController::
    InputDeviceSettingsNotificationController(
        message_center::MessageCenter* message_center)
    : message_center_(message_center) {
  CHECK(message_center_);
}

void InputDeviceSettingsNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* pref_registry) {
  // We'll show the remap to right click and Six Pack keys notifications a
  // total of three times each.
  pref_registry->RegisterIntegerPref(
      prefs::kRemapToRightClickNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyDeleteNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyHomeNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyEndNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyPageUpNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyPageDownNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  pref_registry->RegisterIntegerPref(
      prefs::kSixPackKeyInsertNotificationsRemaining, 3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

InputDeviceSettingsNotificationController::
    ~InputDeviceSettingsNotificationController() = default;

void InputDeviceSettingsNotificationController::
    NotifyRightClickRewriteBlockedBySetting(
        SimulateRightClickModifier blocked_modifier,
        SimulateRightClickModifier active_modifier) {
  CHECK_NE(blocked_modifier, SimulateRightClickModifier::kNone);
  if (!IsActiveUserSession()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);
  int num_notifications_remaining =
      prefs->GetInteger(prefs::kRemapToRightClickNotificationsRemaining);
  if (num_notifications_remaining == 0) {
    return;
  }

  prefs->SetInteger(prefs::kRemapToRightClickNotificationsRemaining,
                    num_notifications_remaining - 1);
  auto on_click_handler =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([]() {
            if (!Shell::Get()->session_controller()->IsUserSessionBlocked()) {
              Shell::Get()
                  ->system_tray_model()
                  ->client()
                  ->ShowTouchpadSettings();
              StopShowingNotification(
                  prefs::kRemapToRightClickNotificationsRemaining);
            }
          }));
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
      message_center::RichNotificationData(), std::move(on_click_handler),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

// TODO(b/279503977): Use `blocked_modifier` and `active_modifier` to display
// the notification message once strings are finalized.
void InputDeviceSettingsNotificationController::
    NotifySixPackRewriteBlockedBySetting(
        ui::KeyboardCode key_code,
        SixPackShortcutModifier blocked_modifier,
        SixPackShortcutModifier active_modifier,
        int device_id) {
  if (!ShouldShowSixPackKeyNotification()) {
    return;
  }
  CHECK_NE(blocked_modifier, SixPackShortcutModifier::kNone);
  CHECK(ui::KeyboardCapability::IsSixPackKey(key_code));

  auto* it = kSixPackKeyToPrefName.find(key_code);
  CHECK(it != kSixPackKeyToPrefName.end());
  const char* pref = it->second;
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);
  int num_notifications_remaining = prefs->GetInteger(pref);
  if (num_notifications_remaining == 0) {
    return;
  }
  prefs->SetInteger(pref, num_notifications_remaining - 1);

  auto on_click_handler =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](int device_id, const char* pref_name) {
                Shell::Get()
                    ->system_tray_model()
                    ->client()
                    ->ShowRemapKeysSubpage(device_id);
                StopShowingNotification(pref_name);
              },
              device_id, pref));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      GetSixPackNotificationId(key_code, device_id),
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_SIX_PACK_KEY,
          GetSixPackKeyName(key_code)),
      std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          NotificationCatalogName::kEventRewriterDeprecation),
      message_center::RichNotificationData(), std::move(on_click_handler),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
