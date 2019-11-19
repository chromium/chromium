// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_screen_extensions_lifetime_manager.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace chromeos {

namespace {

std::vector<std::string> GetLoginScreenPolicyExtensionIds() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());

  const PrefService* const prefs =
      ProfileHelper::GetSigninProfile()->GetPrefs();
  DCHECK_EQ(prefs->GetAllPrefStoresInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_SUCCESS);

  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kLoginScreenExtensions);
  if (!pref || !pref->IsManaged() ||
      pref->GetType() != base::Value::Type::DICTIONARY) {
    return {};
  }
  std::vector<std::string> extension_ids;
  for (const auto& item : pref->GetValue()->DictItems())
    extension_ids.push_back(item.first);
  return extension_ids;
}

void DisableLoginScreenPolicyExtensions() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());

  extensions::ExtensionService* const extension_service =
      extensions::ExtensionSystem::Get(ProfileHelper::GetSigninProfile())
          ->extension_service();
  for (const std::string& extension_id : GetLoginScreenPolicyExtensionIds()) {
    extension_service->DisableExtension(
        extension_id, extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
  }
}

void EnableLoginScreenPolicyExtensions() {
  DCHECK(ProfileHelper::IsSigninProfileInitialized());

  Profile* const signin_profile = ProfileHelper::GetSigninProfile();
  extensions::ExtensionService* const extension_service =
      extensions::ExtensionSystem::Get(signin_profile)->extension_service();
  // This reapplies the policy. For the extensions that were previously disabled
  // due to |DISABLE_BLOCKED_BY_POLICY|, this unsets this disable reason and
  // reenables the extension.
  extension_service->CheckManagementPolicy();

  // Make sure the event pages for the login-screen extensions are loaded back,
  // since they may need to do some operations on the Login/Lock Screen but are
  // likely to have missed the session state change notification.
  extensions::ProcessManager* extensions_process_manager =
      extensions::ProcessManager::Get(signin_profile->GetOriginalProfile());
  for (const std::string& extension_id : GetLoginScreenPolicyExtensionIds())
    extensions_process_manager->WakeEventPage(extension_id, base::DoNothing());
}

bool ShouldEnableLoginScreenPolicyExtensions() {
  // Note that extensions are intentionally allowed during the intermediate
  // transition states between the login screen and the user session, since the
  // user may still see some pieces of the login screen that rely on these
  // extensions.
  return session_manager::SessionManager::Get()->session_state() !=
         session_manager::SessionState::ACTIVE;
}

void UpdateLoginScreenPolicyExtensionsState() {
  if (ShouldEnableLoginScreenPolicyExtensions())
    EnableLoginScreenPolicyExtensions();
  else
    DisableLoginScreenPolicyExtensions();
}

}  // namespace

LoginScreenExtensionsLifetimeManager::LoginScreenExtensionsLifetimeManager() {
  UpdateLoginScreenPolicyExtensionsState();
  session_manager::SessionManager::Get()->AddObserver(this);
  extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile())
      ->AddObserver(this);
}

LoginScreenExtensionsLifetimeManager::~LoginScreenExtensionsLifetimeManager() {
  extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile())
      ->RemoveObserver(this);
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void LoginScreenExtensionsLifetimeManager::OnSessionStateChanged() {
  UpdateLoginScreenPolicyExtensionsState();
}

void LoginScreenExtensionsLifetimeManager::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (extension->location() == extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD &&
      !ShouldEnableLoginScreenPolicyExtensions()) {
    // The policy extensions should be disabled, however the extension got
    // loaded - due to the policy change or due to some internal reason in the
    // extensions subsystem. Therefore forcibly disable this extension.
    extensions::ExtensionSystem::Get(ProfileHelper::GetSigninProfile())
        ->extension_service()
        ->DisableExtension(
            extension->id(),
            extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
  }
}

}  // namespace chromeos
