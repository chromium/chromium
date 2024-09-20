// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/message_center/message_center.h"

namespace ash {

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client)
    : EventRewriterDelegateImpl(
          activation_client,
          std::make_unique<DeprecationNotificationController>(
              message_center::MessageCenter::Get()),
          std::make_unique<InputDeviceSettingsNotificationController>(
              message_center::MessageCenter::Get()),
          InputDeviceSettingsController::Get()) {}

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client,
    std::unique_ptr<DeprecationNotificationController> deprecation_controller,
    std::unique_ptr<InputDeviceSettingsNotificationController>
        input_device_settings_notification_controller,
    InputDeviceSettingsController* input_device_settings_controller)
    : pref_service_for_testing_(nullptr),
      activation_client_(activation_client),
      deprecation_controller_(std::move(deprecation_controller)),
      input_device_settings_notification_controller_(
          std::move(input_device_settings_notification_controller)),
      input_device_settings_controller_(input_device_settings_controller) {}

EventRewriterDelegateImpl::~EventRewriterDelegateImpl() {}

bool EventRewriterDelegateImpl::RewriteModifierKeys() {
  // Do nothing if we have just logged in as guest but have not restarted chrome
  // process yet (so we are still on the login screen). In this situations we
  // have no user profile so can not do anything useful.
  // Note that currently, unlike other accounts, when user logs in as guest, we
  // restart chrome process. In future this is to be changed.
  // TODO(glotov): remove the following condition when we do not restart chrome
  // when user logs in as guest.
  // TODO(kpschoedel): check whether this is still necessary.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      LoginDisplayHost::default_host())
    return false;
  return !suppress_modifier_key_rewrites_;
}

std::optional<ui::mojom::ModifierKey>
EventRewriterDelegateImpl::GetKeyboardRemappedModifierValue(
    int device_id,
    ui::mojom::ModifierKey modifier_key,
    const std::string& pref_name) const {
  // `modifier_key` and `device_id` are unused when the flag is disabled.
  if (!ash::features::IsInputDeviceSettingsSplitEnabled()) {
    if (pref_name.empty()) {
      return std::nullopt;
    }

    // If we're at the login screen, try to get the pref from the global prefs
    // dictionary.
    int value;
    if (LoginDisplayHost::default_host() &&
        LoginDisplayHost::default_host()->GetKeyboardRemappedPrefValue(
            pref_name, &value)) {
      return static_cast<ui::mojom::ModifierKey>(value);
    }
    const PrefService* pref_service = GetPrefService();
    if (!pref_service) {
      return std::nullopt;
    }
    const PrefService::Preference* preference =
        pref_service->FindPreference(pref_name);
    if (!preference) {
      return std::nullopt;
    }

    DCHECK_EQ(preference->GetType(), base::Value::Type::INTEGER);
    return static_cast<ui::mojom::ModifierKey>(
        preference->GetValue()->GetInt());
  }

  // `pref_name` is unused when the flag is enabled.
  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);
  if (!settings) {
    return std::nullopt;
  }

  auto iter = settings->modifier_remappings.find(modifier_key);
  if (iter == settings->modifier_remappings.end()) {
    return modifier_key;
  }

  return iter->second;
}

bool EventRewriterDelegateImpl::TopRowKeysAreFunctionKeys(int device_id) const {
  // When the flag is disabled, `device_id` is unused.
  if (!ash::features::IsInputDeviceSettingsSplitEnabled()) {
    const PrefService* pref_service = GetPrefService();
    if (!pref_service) {
      return false;
    }
    return pref_service->GetBoolean(prefs::kSendFunctionKeys);
  }

  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);
  if (settings) {
    return settings->top_row_are_fkeys;
  }

  if (ash::features::IsPeripheralCustomizationEnabled()) {
    // If it is a mouse or graphics tablet, do not rewrite function keys.
    return input_device_settings_controller_->GetMouseSettings(device_id) ||
           input_device_settings_controller_->GetGraphicsTabletSettings(
               device_id);
  }

  return false;
}

bool EventRewriterDelegateImpl::IsExtensionCommandRegistered(
    ui::KeyboardCode key_code,
    int flags) const {
  if (extension_commands_override_for_testing_.has_value()) {
    return extension_commands_override_for_testing_->count({key_code, flags}) >
           0;
  }

  // Some keyboard events for ChromeOS get rewritten, such as:
  // Search+Shift+Left gets converted to Shift+Home (BeginDocument).
  // This doesn't make sense if the user has assigned that shortcut
  // to an extension. Because:
  // 1) The extension would, upon seeing a request for Ctrl+Shift+Home have
  //    to register for Shift+Home, instead.
  // 2) The conversion is unnecessary, because Shift+Home (BeginDocument) isn't
  //    going to be executed.
  // Therefore, we skip converting the accelerator if an extension has
  // registered for this shortcut.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile || !extensions::ExtensionCommandsGlobalRegistry::Get(profile))
    return false;

  constexpr int kModifierMasks = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                 ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;
  ui::Accelerator accelerator(key_code, flags & kModifierMasks);
  return extensions::ExtensionCommandsGlobalRegistry::Get(profile)
      ->IsRegistered(accelerator);
}

bool EventRewriterDelegateImpl::IsSearchKeyAcceleratorReserved() const {
  // |activation_client_| can be null in test.
  if (!activation_client_)
    return false;

  aura::Window* active_window = activation_client_->GetActiveWindow();
  return active_window &&
         active_window->GetProperty(kSearchKeyAcceleratorReservedKey);
}

bool EventRewriterDelegateImpl::RewriteMetaTopRowKeyComboEvents(
    int device_id) const {
  // When the flag is disabled, `device_id` is unused.
  if (!ash::features::IsInputDeviceSettingsSplitEnabled()) {
    return !suppress_meta_top_row_key_rewrites_;
  }

  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);
  if (settings) {
    return !settings->suppress_meta_fkey_rewrites;
  }

  if (ash::features::IsPeripheralCustomizationEnabled()) {
    // If it is a mouse or graphics tablet, do not rewrite function keys.
    return !(input_device_settings_controller_->GetMouseSettings(device_id) ||
             input_device_settings_controller_->GetGraphicsTabletSettings(
                 device_id));
  }

  return true;
}

void EventRewriterDelegateImpl::SuppressMetaTopRowKeyComboRewrites(
    bool should_suppress) {
  suppress_meta_top_row_key_rewrites_ = should_suppress;
}

void EventRewriterDelegateImpl::RecordEventRemappedToRightClick(
    bool alt_based_right_click) {
  PrefService* const pref_service = GetPrefService();
  if (!pref_service) {
    return;
  }
  const auto* pref_name = alt_based_right_click
                              ? prefs::kAltEventRemappedToRightClick
                              : prefs::kSearchEventRemappedToRightClick;
  int count = pref_service->GetInteger(pref_name);
  pref_service->SetInteger(pref_name, ++count);
}

void EventRewriterDelegateImpl::RecordSixPackEventRewrite(
    ui::KeyboardCode key_code,
    bool alt_based) {
  PrefService* const pref_service = GetPrefService();
  if (!pref_service) {
    return;
  }
  // A map between "six pack" keys to prefs which track how often a user uses
  // either the alt or search based shortcut variant to emit a "six pack" event.
  // The "Insert" key is omitted since the (Search+Shift+Backspace) rewrite is
  // the only way to emit an "Insert" key event.
  static constexpr auto kSixPackKeyToPrefMap =
      base::MakeFixedFlatMap<ui::KeyboardCode, const char*>({
          {ui::KeyboardCode::VKEY_DELETE,
           prefs::kKeyEventRemappedToSixPackDelete},
          {ui::KeyboardCode::VKEY_HOME, prefs::kKeyEventRemappedToSixPackHome},
          {ui::KeyboardCode::VKEY_PRIOR,
           prefs::kKeyEventRemappedToSixPackPageDown},
          {ui::KeyboardCode::VKEY_END, prefs::kKeyEventRemappedToSixPackEnd},
          {ui::KeyboardCode::VKEY_NEXT,
           prefs::kKeyEventRemappedToSixPackPageUp},
      });
  auto it = kSixPackKeyToPrefMap.find(key_code);
  CHECK(it != kSixPackKeyToPrefMap.end());
  int count = pref_service->GetInteger(it->second);
  // `alt_based` tells us whether this "six pack" event was produced by an
  // Alt or Search/Launcher based keyboard shortcut. Update our pref to track
  // which method the user uses more frequently.
  count += alt_based ? 1 : -1;
  pref_service->SetInteger(it->second, count);
}

std::optional<ui::mojom::SimulateRightClickModifier>
EventRewriterDelegateImpl::GetRemapRightClickModifier(int device_id) {
  const mojom::TouchpadSettings* settings =
      input_device_settings_controller_->GetTouchpadSettings(device_id);
  if (!settings) {
    return std::nullopt;
  }
  return settings->simulate_right_click;
}

std::optional<ui::mojom::SixPackShortcutModifier>
EventRewriterDelegateImpl::GetShortcutModifierForSixPackKey(
    int device_id,
    ui::KeyboardCode key_code) {
  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);
  if (!settings) {
    return std::nullopt;
  }
  switch (key_code) {
    case ui::KeyboardCode::VKEY_DELETE:
      return settings->six_pack_key_remappings->del;
    case ui::KeyboardCode::VKEY_HOME:
      return settings->six_pack_key_remappings->home;
    case ui::KeyboardCode::VKEY_PRIOR:
      return settings->six_pack_key_remappings->page_up;
    case ui::KeyboardCode::VKEY_END:
      return settings->six_pack_key_remappings->end;
    case ui::KeyboardCode::VKEY_NEXT:
      return settings->six_pack_key_remappings->page_down;
    case ui::KeyboardCode::VKEY_INSERT:
      return settings->six_pack_key_remappings->insert;
    default:
      NOTREACHED();
  }
}

bool EventRewriterDelegateImpl::NotifyDeprecatedRightClickRewrite() {
  return deprecation_controller_->NotifyDeprecatedRightClickRewrite();
}

bool EventRewriterDelegateImpl::NotifyDeprecatedSixPackKeyRewrite(
    ui::KeyboardCode key_code) {
  return deprecation_controller_->NotifyDeprecatedSixPackKeyRewrite(key_code);
}

PrefService* EventRewriterDelegateImpl::GetPrefService() const {
  if (pref_service_for_testing_)
    return pref_service_for_testing_;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile ? profile->GetPrefs() : nullptr;
}

void EventRewriterDelegateImpl::SuppressModifierKeyRewrites(
    bool should_suppress) {
  suppress_modifier_key_rewrites_ = should_suppress;
}

void EventRewriterDelegateImpl::NotifyRightClickRewriteBlockedBySetting(
    ui::mojom::SimulateRightClickModifier blocked_modifier,
    ui::mojom::SimulateRightClickModifier active_modifier) {
  DCHECK(features::IsAltClickAndSixPackCustomizationEnabled());
  input_device_settings_notification_controller_
      ->NotifyRightClickRewriteBlockedBySetting(blocked_modifier,
                                                active_modifier);
}

void EventRewriterDelegateImpl::NotifySixPackRewriteBlockedBySetting(
    ui::KeyboardCode key_code,
    ui::mojom::SixPackShortcutModifier blocked_modifier,
    ui::mojom::SixPackShortcutModifier active_modifier,
    int device_id) {
  DCHECK(ash::features::IsAltClickAndSixPackCustomizationEnabled());
  input_device_settings_notification_controller_
      ->NotifySixPackRewriteBlockedBySetting(key_code, blocked_modifier,
                                             active_modifier, device_id);
}

std::optional<ui::mojom::ExtendedFkeysModifier>
EventRewriterDelegateImpl::GetExtendedFkeySetting(int device_id,
                                                  ui::KeyboardCode key_code) {
  CHECK(key_code == ui::KeyboardCode::VKEY_F11 ||
        key_code == ui::KeyboardCode::VKEY_F12);

  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);

  if (!settings) {
    return std::nullopt;
  }

  CHECK(settings->f11.has_value() && settings->f12.has_value());
  if (key_code == ui::KeyboardCode::VKEY_F11) {
    return settings->f11;
  }
  return settings->f12;
}

void EventRewriterDelegateImpl::NotifySixPackRewriteBlockedByFnKey(
    ui::KeyboardCode key_code,
    ui::mojom::SixPackShortcutModifier modifier) {
  input_device_settings_notification_controller_->ShowSixPackKeyRewritingNudge(
      key_code, modifier);
}

void EventRewriterDelegateImpl::NotifyTopRowRewriteBlockedByFnKey() {
  input_device_settings_notification_controller_->ShowTopRowRewritingNudge();
}

}  // namespace ash
