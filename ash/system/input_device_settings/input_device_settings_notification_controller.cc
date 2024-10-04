// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include <array>
#include <optional>
#include <string>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

// Needs to stay in sync with `kLargeImageMaxHeight` declared in
// ui/message_center/views/notification_view_md.cc.
const int kMaxNotificationHeight = 218;

int CalculateScaledWidth(int width, int height) {
  return (kMaxNotificationHeight * width) / height;
}

gfx::Image ResizeImage(gfx::ImageSkia image) {
  const SkBitmap bitmap = *image.bitmap();
  SkBitmap bitmap5x =
      skia::ImageOperations::Resize(bitmap, skia::ImageOperations::RESIZE_BEST,
                                    5 * bitmap.width(), 5 * bitmap.height());
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFromBitmap(bitmap5x, 5.0);
  if (image_skia.height() > kMaxNotificationHeight) {
    image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
        image_skia, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(CalculateScaledWidth(image_skia.width(), image_skia.height()),
                  kMaxNotificationHeight));
  }
  return gfx::Image(image_skia);
}
// A nudge/tutorial will not be shown if it already been shown 3 times, or if 24
// hours have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

const char kKeyboardSettingsLearnMoreLink[] =
    "https://support.google.com/chromebook?p=keyboard_settings";
constexpr char kTopRowKeyNoMatchNudgeId[] = "top-row-key-no-match-nudge-id";
constexpr char kSixPackKeyNoMatchNudgeId[] = "six-patch-key-no-match-nudge-id";
constexpr char kCapsLockNoMatchNudgeId[] = "caps-lock-no-match-nudge-id";

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

constexpr auto kKeyCodeToSixPackKeyPrefName =
    base::MakeFixedFlatMap<ui::KeyboardCode, const char*>({
        {ui::KeyboardCode::VKEY_DELETE, {prefs::kSixPackKeyDelete}},
        {ui::KeyboardCode::VKEY_HOME, {prefs::kSixPackKeyHome}},
        {ui::KeyboardCode::VKEY_PRIOR, {prefs::kSixPackKeyPageUp}},
        {ui::KeyboardCode::VKEY_END, {prefs::kSixPackKeyEnd}},
        {ui::KeyboardCode::VKEY_NEXT, {prefs::kSixPackKeyPageDown}},
        {ui::KeyboardCode::VKEY_INSERT, {prefs::kSixPackKeyInsert}},
    });

constexpr auto kKeyCodeToSixPackKeyRemappingNudgeShownCountPref =
    base::MakeFixedFlatMap<ui::KeyboardCode, const char*>({
        {ui::KeyboardCode::VKEY_DELETE,
         {prefs::kDeleteRemappingNudgeShownCount}},
        {ui::KeyboardCode::VKEY_HOME, {prefs::kHomeRemappingNudgeShownCount}},
        {ui::KeyboardCode::VKEY_PRIOR,
         {prefs::kPageUpRemappingNudgeShownCount}},
        {ui::KeyboardCode::VKEY_END, {prefs::kEndRemappingNudgeShownCount}},
        {ui::KeyboardCode::VKEY_NEXT,
         {prefs::kPageDownRemappingNudgeShownCount}},
        {ui::KeyboardCode::VKEY_INSERT,
         {prefs::kInsertRemappingNudgeShownCount}},
    });

constexpr auto kKeyCodeToSixPackKeyRemappingNudgeLastShownPref =
    base::MakeFixedFlatMap<ui::KeyboardCode, const char*>({
        {ui::KeyboardCode::VKEY_DELETE,
         {prefs::kDeleteRemappingNudgeLastShown}},
        {ui::KeyboardCode::VKEY_HOME, {prefs::kHomeRemappingNudgeLastShown}},
        {ui::KeyboardCode::VKEY_PRIOR, {prefs::kPageUpRemappingNudgeLastShown}},
        {ui::KeyboardCode::VKEY_END, {prefs::kEndRemappingNudgeLastShown}},
        {ui::KeyboardCode::VKEY_NEXT,
         {prefs::kPageDownRemappingNudgeLastShown}},
        {ui::KeyboardCode::VKEY_INSERT,
         {prefs::kInsertRemappingNudgeLastShown}},
    });

// Device key of the virtual mouse often used by integration tests, avoid
// showing notification in this case.
const char kVirtualMouseDeviceKey[] = "0000:0000";

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
const char kWelcomeExperienceNotificationPrefix[] = "welcome_experience";
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
      NOTREACHED();
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
      NOTREACHED();
  }
}

std::string GetWelcomeExperienceNotificationId(uint32_t id) {
  return std::string(kWelcomeExperienceNotificationPrefix) + kDelimiter +
         base::NumberToString(id);
}

std::string GetMouseNotificationID(uint32_t id) {
  if (features::IsWelcomeExperienceEnabled()) {
    return GetWelcomeExperienceNotificationId(id);
  }
  return kInputDeviceSettingsMousePrefix + base::NumberToString(id);
}

std::string GetGraphicsTabletNotificationID(uint32_t id) {
  if (features::IsWelcomeExperienceEnabled()) {
    return GetWelcomeExperienceNotificationId(id);
  }
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

bool ShouldBlockNotification() {
  const std::optional<user_manager::UserType> user_type =
      Shell::Get()->session_controller()->GetUserType();
  if (!user_type) {
    return false;
  }

  switch (*user_type) {
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return true;
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return false;
  }
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
      NOTREACHED();
  }
}

std::u16string GetSixPackShortcutUpdatedString(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_PRIOR:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_PAGE_UP_NUDGE_DESCRIPTION);
    case ui::VKEY_NEXT:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_PAGE_DOWN_NUDGE_DESCRIPTION);
    case ui::VKEY_HOME:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_HOME_NUDGE_DESCRIPTION);
    case ui::VKEY_END:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_END_NUDGE_DESCRIPTION);
    case ui::VKEY_DELETE:
      return l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_DELETE_NUDGE_DESCRIPTION);
    default:
      NOTREACHED();
  }
}

void InsertSixPackShortcutKeyboardCodes(
    ui::KeyboardCode key_code,
    std::vector<ui::KeyboardCode>& keyboard_codes) {
  switch (key_code) {
    case ui::VKEY_PRIOR:
      keyboard_codes.push_back(ui::VKEY_UP);
      break;
    case ui::VKEY_NEXT:
      keyboard_codes.push_back(ui::VKEY_DOWN);
      break;
    case ui::VKEY_HOME:
      keyboard_codes.push_back(ui::VKEY_LEFT);
      break;
    case ui::VKEY_END:
      keyboard_codes.push_back(ui::VKEY_RIGHT);
      break;
    case ui::VKEY_DELETE:
      keyboard_codes.push_back(ui::VKEY_BACK);
      break;
    case ui::VKEY_INSERT:
      keyboard_codes.push_back(ui::VKEY_SHIFT);
      keyboard_codes.push_back(ui::VKEY_BACK);
      break;
    default:
      NOTREACHED();
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
      NOTREACHED();
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

void ShowKeyboardSettings() {
  Shell::Get()->system_tray_model()->client()->ShowKeyboardSettings();
}

void ShowGraphicsTabletSettings() {
  Shell::Get()->system_tray_model()->client()->ShowGraphicsTabletSettings();
}

void ShowPointingStickSettings() {
  Shell::Get()->system_tray_model()->client()->ShowPointingStickSettings();
}

void OnLearnMoreClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kKeyboardSettingsLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

// Compares button remapping lists to see if they are equal for notification
// purposes. This ignores name mismatches, but compares everything else
// (including ordering).
bool ButtonRemappingListsAreEqual(
    const std::vector<mojom::ButtonRemappingPtr>& button_remappings1,
    const std::vector<mojom::ButtonRemappingPtr>& button_remappings2) {
  if (button_remappings1.size() != button_remappings2.size()) {
    return false;
  }

  for (size_t i = 0; i < button_remappings1.size(); i++) {
    const auto& button_remapping1 = button_remappings1[i];
    const auto& button_remapping2 = button_remappings2[i];
    if (button_remapping1->button != button_remapping2->button ||
        button_remapping1->remapping_action !=
            button_remapping2->remapping_action) {
      return false;
    }
  }

  return true;
}

const std::u16string GetBatteryLevelMessage(
    const mojom::BatteryInfo& battery_info) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_BATTERY_DESCRIPTION,
      base::NumberToString16(battery_info.battery_percentage));
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
  pref_registry->RegisterIntegerPref(prefs::kCapsLockRemappingNudgeShownCount,
                                     0);
  pref_registry->RegisterIntegerPref(prefs::kTopRowRemappingNudgeShownCount, 0);
  pref_registry->RegisterTimePref(prefs::kCapsLockRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterTimePref(prefs::kTopRowRemappingNudgeLastShown,
                                  base::Time());
  pref_registry->RegisterListPref(prefs::kPeripheralNotificationMiceSeen);
  pref_registry->RegisterListPref(
      prefs::kPeripheralNotificationGraphicsTabletsSeen);
  pref_registry->RegisterListPref(prefs::kWelcomeExperienceNotificationSeen);
  pref_registry->RegisterDictionaryPref(
      prefs::kKeyboardSettingSixPackKeyRemappings);
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
    const mojom::Mouse& mouse,
    const gfx::ImageSkia& device_image) {
  if (!IsActiveUserSession() || !mouse.is_external ||
      ShouldBlockNotification()) {
    return;
  }

  // Avoid showing notification for the virtual mouse device.
  if (mouse.device_key == kVirtualMouseDeviceKey) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  const char* pref_name = features::IsWelcomeExperienceEnabled()
                              ? prefs::kWelcomeExperienceNotificationSeen
                              : prefs::kPeripheralNotificationMiceSeen;
  if (base::Contains(prefs->GetList(pref_name), mouse.device_key)) {
    return;
  }

  auto seen_device_list = prefs->GetList(pref_name).Clone();

  seen_device_list.Append(mouse.device_key);
  prefs->SetList(pref_name, std::move(seen_device_list));

  CHECK(mouse.settings);
  // Do not show notification if the device remapping list has already been
  // changed which means they have already used the customization feature.
  if (!ButtonRemappingListsAreEqual(
          GetButtonRemappingListForConfig(mouse.mouse_button_config),
          mouse.settings->button_remappings)) {
    return;
  }
  NotifyMouseIsCustomizable(mouse, device_image);
}

void InputDeviceSettingsNotificationController::
    NotifyGraphicsTabletFirstTimeConnected(
        const mojom::GraphicsTablet& graphics_tablet,
        const gfx::ImageSkia& device_image) {
  if (!IsActiveUserSession() || ShouldBlockNotification()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  const char* pref_name =
      features::IsWelcomeExperienceEnabled()
          ? prefs::kWelcomeExperienceNotificationSeen
          : prefs::kPeripheralNotificationGraphicsTabletsSeen;

  auto seen_device_list = prefs->GetList(pref_name).Clone();

  for (const auto& value : seen_device_list) {
    if (value.is_string() && value.GetString() == graphics_tablet.device_key) {
      return;
    }
  }
  seen_device_list.Append(graphics_tablet.device_key);
  prefs->SetList(pref_name, std::move(seen_device_list));

  CHECK(graphics_tablet.settings);
  // Do not show notification if the device remapping list has already been
  // changed which means they have already used the customization feature.
  if (!ButtonRemappingListsAreEqual(
          GetPenButtonRemappingListForConfig(
              graphics_tablet.graphics_tablet_button_config),
          graphics_tablet.settings->pen_button_remappings) ||
      !ButtonRemappingListsAreEqual(
          GetTabletButtonRemappingListForConfig(
              graphics_tablet.graphics_tablet_button_config),
          graphics_tablet.settings->tablet_button_remappings)) {
    return;
  }
  NotifyGraphicsTabletIsCustomizable(graphics_tablet, device_image);
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
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kClicked);
  RemoveNotification(notification_id);
  return;
}

void HandleKeyboardCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowKeyboardSettings();
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kClicked);
  RemoveNotification(notification_id);
  return;
}

void HandleTouchpadCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowTouchpadSettings();
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kClicked);
  RemoveNotification(notification_id);
  return;
}

void HandlePointingStickCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowPointingStickSettings();
  RemoveNotification(notification_id);
  return;
}

void HandleGraphicsTabletCustomizationNotificationClicked(
    const std::string& notification_id,
    std::optional<int> button_index) {
  ShowGraphicsTabletSettings();
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kClicked);
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

  auto it = kSixPackKeyToPrefName.find(key_code);
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

void InputDeviceSettingsNotificationController::
    NotifyKeyboardFirstTimeConnected(const mojom::Keyboard& keyboard,
                                     const gfx::ImageSkia& device_image) {
  if (!IsActiveUserSession() || !keyboard.is_external ||
      ShouldBlockNotification()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  if (base::Contains(prefs->GetList(prefs::kWelcomeExperienceNotificationSeen),
                     keyboard.device_key)) {
    return;
  }

  auto seen_device_list =
      prefs->GetList(prefs::kWelcomeExperienceNotificationSeen).Clone();

  seen_device_list.Append(keyboard.device_key);
  prefs->SetList(prefs::kWelcomeExperienceNotificationSeen,
                 std::move(seen_device_list));

  CHECK(keyboard.settings);
  ShowKeyboardSettingsNotification(keyboard, device_image);
}

void InputDeviceSettingsNotificationController::
    NotifyTouchpadFirstTimeConnected(const mojom::Touchpad& touchpad,
                                     const gfx::ImageSkia& device_image) {
  if (!IsActiveUserSession() || !touchpad.is_external ||
      ShouldBlockNotification()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  if (base::Contains(prefs->GetList(prefs::kWelcomeExperienceNotificationSeen),
                     touchpad.device_key)) {
    return;
  }

  auto seen_device_list =
      prefs->GetList(prefs::kWelcomeExperienceNotificationSeen).Clone();

  seen_device_list.Append(touchpad.device_key);
  prefs->SetList(prefs::kWelcomeExperienceNotificationSeen,
                 std::move(seen_device_list));

  CHECK(touchpad.settings);
  ShowTouchpadSettingsNotification(touchpad, device_image);
}

void InputDeviceSettingsNotificationController::
    ShowPointingStickSettingsNotification(
        const mojom::PointingStick& pointing_stick) {
  const auto peripheral_name = base::UTF8ToUTF16(pointing_stick.name);
  const auto notification_id =
      GetWelcomeExperienceNotificationId(pointing_stick.id);
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_POINTING_STICK_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_POINTING_STICK,
          peripheral_name),
      std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &HandlePointingStickCustomizationNotificationClicked,
              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::
    NotifyPointingStickFirstTimeConnected(
        const mojom::PointingStick& pointing_stick) {
  if (!IsActiveUserSession() || !pointing_stick.is_external ||
      ShouldBlockNotification()) {
    return;
  }

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  if (base::Contains(prefs->GetList(prefs::kWelcomeExperienceNotificationSeen),
                     pointing_stick.device_key)) {
    return;
  }

  auto seen_device_list =
      prefs->GetList(prefs::kWelcomeExperienceNotificationSeen).Clone();

  seen_device_list.Append(pointing_stick.device_key);
  prefs->SetList(prefs::kWelcomeExperienceNotificationSeen,
                 std::move(seen_device_list));

  CHECK(pointing_stick.settings);
  ShowPointingStickSettingsNotification(pointing_stick);
}

void InputDeviceSettingsNotificationController::NotifyMouseIsCustomizable(
    const mojom::Mouse& mouse,
    const gfx::ImageSkia& device_image) {
  const auto peripheral_name = base::UTF8ToUTF16(mouse.name);
  const auto notification_id = GetMouseNotificationID(mouse.id);
  const auto message =
      mouse.battery_info.is_null()
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_MOUSE_CUSTOMIZATION,
                peripheral_name)
          : GetBatteryLevelMessage(*mouse.battery_info);
  message_center::RichNotificationData rich_notification_data;
  if (!device_image.isNull()) {
    rich_notification_data.image = ResizeImage(device_image);
  }
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_PERIPHERAL_CUSTOMIZATION_TITLE),
      message, std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleMouseCustomizationNotificationClicked,
                              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  notification_id_to_device_key_map_[notification_id] = mouse.device_key;
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kShown);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::
    ShowKeyboardSettingsNotification(const mojom::Keyboard& keyboard,
                                     const gfx::ImageSkia& device_image) {
  const auto peripheral_name = base::UTF8ToUTF16(keyboard.name);
  const auto notification_id = GetWelcomeExperienceNotificationId(keyboard.id);
  const auto message =
      keyboard.battery_info.is_null()
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_KEYBOARD,
                peripheral_name)
          : GetBatteryLevelMessage(*keyboard.battery_info);
  message_center::RichNotificationData rich_notification_data;
  if (!device_image.isNull()) {
    rich_notification_data.image = ResizeImage(device_image);
  }
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_KEYBOARD_TITLE),
      message, std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleKeyboardCustomizationNotificationClicked,
                              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  notification_id_to_device_key_map_[notification_id] = keyboard.device_key;
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kShown);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::
    ShowTouchpadSettingsNotification(const mojom::Touchpad& touchpad,
                                     const gfx::ImageSkia& device_image) {
  const auto peripheral_name = base::UTF8ToUTF16(touchpad.name);
  const auto message =
      touchpad.battery_info.is_null()
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_TOUCHPAD,
                peripheral_name)
          : GetBatteryLevelMessage(*touchpad.battery_info);
  const auto notification_id = GetWelcomeExperienceNotificationId(touchpad.id);
  message_center::RichNotificationData rich_notification_data;
  if (!device_image.isNull()) {
    rich_notification_data.image = ResizeImage(device_image);
  }
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_WELCOME_EXPERIENCE_TOUCHPAD_TITLE),
      message, std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HandleTouchpadCustomizationNotificationClicked,
                              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  notification_id_to_device_key_map_[notification_id] = touchpad.device_key;
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kShown);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::
    NotifyGraphicsTabletIsCustomizable(
        const mojom::GraphicsTablet& graphics_tablet,
        const gfx::ImageSkia& device_image) {
  const auto peripheral_name = base::UTF8ToUTF16(graphics_tablet.name);
  const auto message =
      graphics_tablet.battery_info.is_null()
          ? l10n_util::GetStringFUTF16(
                IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_GRAPHICS_TABLET_CUSTOMIZATION,
                peripheral_name)
          : GetBatteryLevelMessage(*graphics_tablet.battery_info);
  const auto notification_id =
      GetGraphicsTabletNotificationID(graphics_tablet.id);
  message_center::RichNotificationData rich_notification_data;
  if (!device_image.isNull()) {
    rich_notification_data.image = ResizeImage(device_image);
  }
  rich_notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_OPEN_SETTINGS_BUTTON));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_ASH_DEVICE_SETTINGS_NOTIFICATIONS_PERIPHERAL_CUSTOMIZATION_GRAPHICS_TABLET_TITLE),
      message, std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kInputDeviceSettings),
      rich_notification_data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &HandleGraphicsTabletCustomizationNotificationClicked,
              notification_id)),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  notification_id_to_device_key_map_[notification_id] =
      graphics_tablet.device_key;
  base::UmaHistogramEnumeration(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kShown);
  message_center_->AddNotification(std::move(notification));
}

void InputDeviceSettingsNotificationController::ShowTopRowRewritingNudge() {
  if (!IsActiveUserSession()) {
    return;
  }

  CHECK(ash::Shell::HasInstance() && Shell::Get()->session_controller());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  const int shown_count =
      prefs->GetInteger(prefs::kTopRowRemappingNudgeShownCount);
  const base::Time last_shown_time =
      prefs->GetTime(prefs::kTopRowRemappingNudgeLastShown);
  // Do not show the nudge more than three times, or if it has already been
  // shown in the past 24 hours.
  const base::Time now = base::Time::Now();
  if ((shown_count >= kNudgeMaxShownCount) ||
      ((now - last_shown_time) < kNudgeTimeBetweenShown)) {
    return;
  }

  prefs->SetInteger(prefs::kTopRowRemappingNudgeShownCount, shown_count + 1);
  prefs->SetTime(prefs::kTopRowRemappingNudgeLastShown, now);

  AnchoredNudgeData nudge_data(
      kTopRowKeyNoMatchNudgeId, NudgeCatalogName::kSearchTopRowKeyPressed,
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_TOP_ROW_NUDGE_DESCRIPTION));
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_KEYBOARD_FN_KEY_NUDGE_IMAGE);

  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void InputDeviceSettingsNotificationController::ShowSixPackKeyRewritingNudge(
    ui::KeyboardCode key_code,
    SixPackShortcutModifier old_matched_modifier) {
  if (!IsActiveUserSession() ||
      !ui::KeyboardCapability::IsSixPackKey(key_code)) {
    return;
  }

  // Insert does not have a notification to show even though it is a six-pack
  // key.
  if (key_code == ui::VKEY_INSERT) {
    return;
  }

  CHECK(ash::Shell::HasInstance() && Shell::Get()->session_controller());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  const auto* six_pack_key_remappings =
      prefs->GetDict(prefs::kKeyboardDefaultChromeOSSettings)
          .FindDict(prefs::kKeyboardSettingSixPackKeyRemappings);

  std::optional<int> six_pack_key_modifier =
      static_cast<int>(SixPackShortcutModifier::kSearch);

  // Only show the notification if the modifier key matches the pref in user's
  // last device for the behavior.
  if (six_pack_key_remappings) {
    auto it = kKeyCodeToSixPackKeyPrefName.find(key_code);
    CHECK(it != kKeyCodeToSixPackKeyPrefName.end());
    const char* pref = it->second;

    six_pack_key_modifier = six_pack_key_remappings->FindInt(pref);
    if (six_pack_key_modifier != std::nullopt &&
        six_pack_key_modifier != static_cast<int>(old_matched_modifier)) {
      return;
    }
  }

  const auto shown_count_pref_iter =
      kKeyCodeToSixPackKeyRemappingNudgeShownCountPref.find(key_code);
  CHECK(shown_count_pref_iter !=
        kKeyCodeToSixPackKeyRemappingNudgeShownCountPref.end());
  const char* shown_count_pref_name = shown_count_pref_iter->second;
  const int shown_count = prefs->GetInteger(shown_count_pref_name);

  const auto last_shown_time_iter =
      kKeyCodeToSixPackKeyRemappingNudgeLastShownPref.find(key_code);
  CHECK(last_shown_time_iter !=
        kKeyCodeToSixPackKeyRemappingNudgeLastShownPref.end());
  const char* last_shown_time_pref_name = last_shown_time_iter->second;
  const base::Time last_shown_time = prefs->GetTime(last_shown_time_pref_name);

  // Do not show the nudge more than three times, or if it has already been
  // shown in the past 24 hours.
  const base::Time now = base::Time::Now();
  if ((shown_count >= kNudgeMaxShownCount) ||
      ((now - last_shown_time) < kNudgeTimeBetweenShown)) {
    return;
  }

  prefs->SetInteger(shown_count_pref_name, shown_count + 1);
  prefs->SetTime(last_shown_time_pref_name, now);

  AnchoredNudgeData nudge_data(kSixPackKeyNoMatchNudgeId,
                               NudgeCatalogName::kSixPackRemappingPressed,
                               GetSixPackShortcutUpdatedString(key_code));
  std::vector<ui::KeyboardCode> keyboard_codes = {ui::VKEY_FUNCTION};
  InsertSixPackShortcutKeyboardCodes(key_code, keyboard_codes);
  nudge_data.keyboard_codes = std::move(keyboard_codes);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_KEYBOARD_FN_KEY_NUDGE_IMAGE);
  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void InputDeviceSettingsNotificationController::ShowCapsLockRewritingNudge() {
  if (!IsActiveUserSession()) {
    return;
  }

  CHECK(ash::Shell::HasInstance() && Shell::Get()->session_controller());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  const int shown_count =
      prefs->GetInteger(prefs::kCapsLockRemappingNudgeShownCount);
  const base::Time last_shown_time =
      prefs->GetTime(prefs::kCapsLockRemappingNudgeLastShown);
  // Do not show the nudge more than three times, or if it has already been
  // shown in the past 24 hours.
  const base::Time now = base::Time::Now();
  if ((shown_count >= kNudgeMaxShownCount) ||
      ((now - last_shown_time) < kNudgeTimeBetweenShown)) {
    return;
  }

  prefs->SetInteger(prefs::kCapsLockRemappingNudgeShownCount, shown_count + 1);
  prefs->SetTime(prefs::kCapsLockRemappingNudgeLastShown, now);

  AnchoredNudgeData nudge_data(
      kCapsLockNoMatchNudgeId, NudgeCatalogName::kCapsLockShortcutPressed,
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_CAPS_LOCK_NUDGE_DESCRIPTION));
  nudge_data.keyboard_codes = {ui::VKEY_FUNCTION, ui::VKEY_RIGHT_ALT};
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_KEYBOARD_CAPSLOCK_KEY_NUDGE_IMAGE);

  AnchoredNudgeManager::Get()->Show(nudge_data);
}

std::optional<std::string>
InputDeviceSettingsNotificationController::GetDeviceKeyForNotificationId(
    const std::string& notification_id) {
  auto it = notification_id_to_device_key_map_.find(notification_id);
  if (it == notification_id_to_device_key_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace ash
