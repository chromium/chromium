// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/message_center/message_center.h"

namespace ash {

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client)
    : EventRewriterDelegateImpl(
          activation_client,
          std::make_unique<DeprecationNotificationController>(
              message_center::MessageCenter::Get())) {}

EventRewriterDelegateImpl::EventRewriterDelegateImpl(
    wm::ActivationClient* activation_client,
    std::unique_ptr<DeprecationNotificationController> deprecation_controller)
    : pref_service_for_testing_(nullptr),
      activation_client_(activation_client),
      deprecation_controller_(std::move(deprecation_controller)) {}

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

bool EventRewriterDelegateImpl::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) const {
  DCHECK(value);
  // If we're at the login screen, try to get the pref from the global prefs
  // dictionary.
  if (LoginDisplayHost::default_host() &&
      LoginDisplayHost::default_host()->GetKeyboardRemappedPrefValue(pref_name,
                                                                     value)) {
    return true;
  }
  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return false;
  const PrefService::Preference* preference =
      pref_service->FindPreference(pref_name);
  if (!preference)
    return false;

  DCHECK_EQ(preference->GetType(), base::Value::Type::INTEGER);
  *value = preference->GetValue()->GetInt();
  return true;
}

bool EventRewriterDelegateImpl::TopRowKeysAreFunctionKeys() const {
  const PrefService* pref_service = GetPrefService();
  if (!pref_service)
    return false;
  return pref_service->GetBoolean(prefs::kSendFunctionKeys);
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
