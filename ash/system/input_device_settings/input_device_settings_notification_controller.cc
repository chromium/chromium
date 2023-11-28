// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include <array>
#include <string>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
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

const char kKeyboardSettingsLearnMoreLink[] =
    "https://support.google.com/chromebook?p=keyboard_settings";

using SimulateRightClickModifier = ui::mojom::SimulateRightClickModifier;
using SixPackShortcutModifier = ui::mojom::SixPackShortcutModifier;

// Maps a six pack key to the search/alt shortcut strings.
static constexpr auto kSixPackNotificationsMap =
    base::MakeFixedFlatMap<ui::KeyboardCode, std::array<int, 2>>({
        {ui::KeyboardCode::VKEY_DELETE,
         {IDS_ASH_SETTINGS_SIX_PACK_KEY_DELETE_ALT,
          IDS_ASH_SETTINGS_SIX_PACK_KEY_DELETE_SEARCH}},
        {ui::KeyboardCode::VKEY_HOME,
         {IDS_ASH_SETTINGS_SIX_PACK_KEY_HOME_ALT,
          IDS_ASH_SETTINGS_SIX_PACK_KEY_HOME_SEARCH}},
        {ui::KeyboardCode::VKEY_END,
         {IDS_ASH_SETTINGS_SIX_PACK_KEY_END_ALT,
          IDS_ASH_SETTINGS_SIX_PACK_KEY_END_SEARCH}},
        {ui::KeyboardCode::VKEY_NEXT,
         {IDS_ASH_SETTINGS_SIX_PACK_KEY_PAGE_DOWN_ALT,
          IDS_ASH_SETTINGS_SIX_PACK_KEY_PAGE_DOWN_SEARCH}},
        {ui::KeyboardCode::VKEY_PRIOR,
         {IDS_ASH_SETTINGS_SIX_PACK_KEY_PAGE_UP_ALT,
          IDS_ASH_SETTINGS_SIX_PACK_KEY_PAGE_UP_SEARCH}},
    });

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
const char kInputDeviceSettingsMousePrefix[] =
    "peripheral_customization_mouse_";
const char kInputDeviceSettingsGraphicsTabletPrefix[] =
    "peripheral_customization_graphics_tablet_";
const char kDelimiter[] = "_";

bool IsRightClickRewriteDisabled(SimulateRightClickModifier active_modifier) {
  return active_modifier == SimulateRightClickModifier::kNone;
}

bool IsSixPackShortcutDisabled(SixPackShortcutModifier active_modifier) {
  return active_modifier == SixPackShortcutModifier::kNone;
}

std::u16string GetRightClickRewriteNotificationMessage(
    SimulateRightClickModifier blocked_modifier,
    SimulateRightClickModifier active_modifier) {
  if (IsRightClickRewriteDisabled(active_modifier)) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_RIGHT_CLICK_DISABLED);
  }

  const int launcher_key_name_id =
      Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
          ? IDS_ASH_SETTINGS_SHORTCUT_MODIFIER_LAUNCHER
          : IDS_ASH_SETTINGS_SHORTCUT_MODIFIER_SEARCH;
  const std::u16string launcher_key_name =
      l10n_util::GetStringUTF16(launcher_key_name_id);

  switch (blocked_modifier) {
    case SimulateRightClickModifier::kAlt:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_ALT_RIGHT_CLICK,
          launcher_key_name);
    case SimulateRightClickModifier::kSearch:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_LAUNCHER_RIGHT_CLICK,
          launcher_key_name);
    case SimulateRightClickModifier::kNone:
      NOTREACHED_NORETURN();
  }
}

std::u16string GetRightClickRewriteNotificationTitle(
    SimulateRightClickModifier active_modifier) {
  if (IsRightClickRewriteDisabled(active_modifier)) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_RIGHT_CLICK_DISABLED_TITLE);
  }
  return l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_RIGHT_CLICK_TITLE);
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

std::string GetPeripheralCustomizationMouseNotificationID(uint32_t id) {
  return kInputDeviceSettingsMousePrefix + base::NumberToString(id);
}

std::string GetPeripheralCustomizationGraphicsTabletNotificationID(
    uint32_t id) {
  return kInputDeviceSettingsGraphicsTabletPrefix + base::NumberToString(id);
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
void PreventNotificationFromShowingAgain(const char* pref_name) {
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

std::u16string GetSixPackShortcut(ui::KeyboardCode key_code,
                                  SixPackShortcutModifier modifier) {
  CHECK(modifier != SixPackShortcutModifier::kNone);
  int message_id =
      kSixPackNotificationsMap.at(key_code)[static_cast<int>(modifier) - 1];

  if (modifier == SixPackShortcutModifier::kSearch) {
    const int launcher_key_name_id =
        Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
            ? IDS_ASH_SETTINGS_SHORTCUT_MODIFIER_LAUNCHER
            : IDS_ASH_SETTINGS_SHORTCUT_MODIFIER_SEARCH;
    const std::u16string launcher_key_name =
        l10n_util::GetStringUTF16(launcher_key_name_id);
    return l10n_util::GetStringFUTF16(message_id, launcher_key_name);
  }
  return l10n_util::GetStringUTF16(message_id);
}

std::u16string GetSixPackNotificationMessage(
    ui::KeyboardCode key_code,
    SixPackShortcutModifier blocked_modifier,
    SixPackShortcutModifier active_modifier) {
  const auto six_pack_key_name = GetSixPackKeyName(key_code);
  if (IsSixPackShortcutDisabled(active_modifier)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_SIX_PACK_SHORTCUT_DISABLED,
        six_pack_key_name);
  }

  // There's only one active shortcut for the "Insert" six pack key.
  CHECK(key_code != ui::KeyboardCode::VKEY_INSERT);
  const auto new_shortcut = GetSixPackShortcut(key_code, active_modifier);
  const auto old_shortcut = GetSixPackShortcut(key_code, blocked_modifier);
  return l10n_util::GetStringFUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_SIX_PACK_KEY, six_pack_key_name,
      new_shortcut, old_shortcut);
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

void RemoveNotification(const std::string& notification_id) {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*by_user=*/true);
}

void ShowRemapKeysSubpage(int device_id) {
  Shell::Get()->system_tray_model()->client()->ShowRemapKeysSubpage(device_id);
}

void ShowTouchpadSettings() {
  Shell::Get()->system_tray_model()->client()->ShowTouchpadSettings();
}

void ShowMouseSettings() {
  Shell::Get()->system_tray_model()->client()->ShowMouseSettings();
}

void ShowGraphicsTabletSettings() {
  Shell::Get()->system_tray_model()->client()->ShowGraphicsTabletSettings();
}

void OnLearnMoreClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kKeyboardSettingsLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
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
  pref_registry->RegisterListPref(prefs::kPeripheralNotificationMiceSeen);
  pref_registry->RegisterListPref(
      prefs::kPeripheralNotificationGraphicsTabletsSeen);
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
  const auto notification_id =
      GetRightClickNotificationId(blocked_modifier, active_modifier);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SETTINGS_EDIT_SHORTCUT_BUTTON));
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SETTINGS_LEARN_MORE_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      GetRightClickRewriteNotificationTitle(active_modifier),
      GetRightClickRewriteNotificationMessage(blocked_modifier,
                                              active_modifier),
      std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          NotificationCatalogName::kEventRewriterDeprecation),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&InputDeviceSettingsNotificationController::
                                  HandleRightClickNotificationClicked,
                              weak_ptr_factory_.GetWeakPtr(), notification_id)),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::NotifyMouseFirstTimeConnected(
    const mojom::Mouse& mouse) {
  if (!IsActiveUserSession()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  if (base::Contains(prefs->GetList(prefs::kPeripheralNotificationMiceSeen),
                     mouse.device_key)) {
    return;
  }

  auto seen_mouse_list =
      prefs->GetList(prefs::kPeripheralNotificationMiceSeen).Clone();

  seen_mouse_list.Append(mouse.device_key);
  prefs->SetList(prefs::kPeripheralNotificationMiceSeen,
                 std::move(seen_mouse_list));
  NotifyMouseIsCustomizable(mouse);
}

void InputDeviceSettingsNotificationController::
    NotifyGraphicsTabletFirstTimeConnected(
        const mojom::GraphicsTablet* graphics_tablet) {
  if (!IsActiveUserSession()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  auto seen_graphics_tablet_list =
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).Clone();

  for (const auto& value : seen_graphics_tablet_list) {
    if (value.is_string() && value.GetString() == graphics_tablet->device_key) {
      return;
    }
  }
  seen_graphics_tablet_list.Append(graphics_tablet->device_key);
  prefs->SetList(prefs::kPeripheralNotificationGraphicsTabletsSeen,
                 std::move(seen_graphics_tablet_list));
  NotifyGraphicsTabletIsCustomizable(*graphics_tablet);
}

void InputDeviceSettingsNotificationController::
    HandleSixPackNotificationClicked(int device_id,
                                     const char* pref_name,
                                     const std::string& notification_id,
                                     std::optional<int> button_index) {
  // Clicked on body.
  if (!button_index) {
    ShowRemapKeysSubpage(device_id);
    RemoveNotification(notification_id);
    PreventNotificationFromShowingAgain(pref_name);
    return;
  }

  switch (*button_index) {
    case NotificationButtonIndex::BUTTON_EDIT_SHORTCUT:
      ShowRemapKeysSubpage(device_id);
      break;
    case NotificationButtonIndex::BUTTON_LEARN_MORE:
      OnLearnMoreClicked();
      break;
  }
  PreventNotificationFromShowingAgain(pref_name);
  RemoveNotification(notification_id);
}

void InputDeviceSettingsNotificationController::
    HandleRightClickNotificationClicked(const std::string& notification_id,
                                        std::optional<int> button_index) {
  // Clicked on body.
  if (!button_index) {
    ShowTouchpadSettings();
    PreventNotificationFromShowingAgain(
        prefs::kRemapToRightClickNotificationsRemaining);
    RemoveNotification(notification_id);
    return;
  }

  switch (*button_index) {
    case NotificationButtonIndex::BUTTON_EDIT_SHORTCUT:
      ShowTouchpadSettings();
      break;
    case NotificationButtonIndex::BUTTON_LEARN_MORE:
      OnLearnMoreClicked();
      break;
  }
  PreventNotificationFromShowingAgain(
      prefs::kRemapToRightClickNotificationsRemaining);
  RemoveNotification(notification_id);
}

void HandleMouseCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowMouseSettings();
  RemoveNotification(notification_id);
  return;
}

void HandleGraphicsTabletCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowGraphicsTabletSettings();
  RemoveNotification(notification_id);
  return;
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

  const auto notification_id = GetSixPackNotificationId(key_code, device_id);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SETTINGS_EDIT_SHORTCUT_BUTTON));
  rich_notification_data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_DEVICE_SETTINGS_LEARN_MORE_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(IDS_ASH_SETTINGS_SHORTCUT_NOTIFICATION_TITLE),
      GetSixPackNotificationMessage(key_code, blocked_modifier,
                                    active_modifier),
      std::u16string(), GURL(),
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
          NotificationCatalogName::kEventRewriterDeprecation),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&InputDeviceSettingsNotificationController::
                                  HandleSixPackNotificationClicked,
                              weak_ptr_factory_.GetWeakPtr(), device_id, pref,
                              notification_id)),
      kNotificationKeyboardIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::NotifyMouseIsCustomizable(
    const mojom::Mouse& mouse) {
  const auto peripheral_name = base::UTF8ToUTF16(mouse.name);
  const auto notification_id =
      GetPeripheralCustomizationMouseNotificationID(mouse.id);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_PERIPHERAL_CUSTOMIZATION_TITLE,
          peripheral_name),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_MOUSE_CUSTOMIZATION,
          peripheral_name),
      std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleMouseCustomizationNotificationClicked,
                              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::
    NotifyGraphicsTabletIsCustomizable(
        const mojom::GraphicsTablet& graphics_tablet) {
  const auto peripheral_name = base::UTF8ToUTF16(graphics_tablet.name);
  const auto notification_id =
      GetPeripheralCustomizationGraphicsTabletNotificationID(
          graphics_tablet.id);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_PERIPHERAL_CUSTOMIZATION_TITLE,
          peripheral_name),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_GRAPHICS_TABLET_CUSTOMIZATION,
          peripheral_name),
      std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &HandleGraphicsTabletCustomizationNotificationClicked,
              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

}  // namespace ash
