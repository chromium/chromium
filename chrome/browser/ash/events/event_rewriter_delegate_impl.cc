// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom-shared.h"
#include "ui/message_center/message_center.h"

namespace ash {

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client)
    : EventRewriterDelegateImpl(
          activation_client,
          std::make_unique<DeprecationNotificationController>(
              message_center::MessageCenter::Get()),
          InputDeviceSettingsController::Get()) {}

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client,
    std::unique_ptr<DeprecationNotificationController> deprecation_controller,
    InputDeviceSettingsController* input_device_settings_controller)
    : pref_service_for_testing_(nullptr),
      activation_client_(activation_client),
      deprecation_controller_(std::move(deprecation_controller)),
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

absl::optional<ui::mojom::ModifierKey>
EventRewriterDelegateImpl::GetKeyboardRemappedModifierValue(
    int device_id,
    ui::mojom::ModifierKey modifier_key,
    const std::string& pref_name) const {
  // `modifier_key` and `device_id` are unused when the flag is disabled.
  if (!ash::features::IsInputDeviceSettingsSplitEnabled()) {
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
      return absl::nullopt;
    }
    const PrefService::Preference* preference =
        pref_service->FindPreference(pref_name);
    if (!preference) {
      return absl::nullopt;
    }

    DCHECK_EQ(preference->GetType(), base::Value::Type::INTEGER);
    return static_cast<ui::mojom::ModifierKey>(
        preference->GetValue()->GetInt());
  }

  // `pref_name` is unused when the flag is enabled.
  const mojom::KeyboardSettings* settings =
      input_device_settings_controller_->GetKeyboardSettings(device_id);
  if (!settings) {
    return modifier_key;
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
  // TODO(dpad): Add metric for when settings are not able to be found.
  return settings && settings->top_row_are_fkeys;
}

bool EventRewriterDelegateImpl::IsExtensionCommandRegistered(
    ui::KeyboardCode key_code,
    int flags) const {
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
  // TODO(dpad): Add metric for when settings are not able to be found.
  return !(settings && settings->suppress_meta_fkey_rewrites);
}

void EventRewriterDelegateImpl::SuppressMetaTopRowKeyComboRewrites(
    bool should_suppress) {
  suppress_meta_top_row_key_rewrites_ = should_suppress;
}

bool EventRewriterDelegateImpl::NotifyDeprecatedRightClickRewrite() {
  return deprecation_controller_->NotifyDeprecatedRightClickRewrite();
}

bool EventRewriterDelegateImpl::NotifyDeprecatedSixPackKeyRewrite(
    ui::KeyboardCode key_code) {
  return deprecation_controller_->NotifyDeprecatedSixPackKeyRewrite(key_code);
}

const PrefService* EventRewriterDelegateImpl::GetPrefService() const {
  if (pref_service_for_testing_)
    return pref_service_for_testing_;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile ? profile->GetPrefs() : nullptr;
}

void EventRewriterDelegateImpl::SuppressModifierKeyRewrites(
    bool should_suppress) {
  suppress_modifier_key_rewrites_ = should_suppress;
}
}  // namespace ash
