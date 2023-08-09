// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

namespace ash {

LoginScreenExtensionsLifetimeManager::LoginScreenExtensionsLifetimeManager(
    Profile* signin_original_profile)
    : signin_original_profile_(signin_original_profile),
      extension_system_(
          extensions::ExtensionSystem::Get(signin_original_profile_)),
      extensions_process_manager_(
          extensions::ProcessManager::Get(signin_original_profile_)),
      session_manager_(session_manager::SessionManager::Get()) {
  DCHECK(signin_original_profile_);
  DCHECK(extension_system_);
  DCHECK(extensions_process_manager_);
  DCHECK(session_manager_);

  auto* const extension_registry =
      extensions::ExtensionRegistry::Get(signin_original_profile_);
  DCHECK(extension_registry);
  extension_registry_observation_.Observe(extension_registry);

  session_manager_observation_.Observe(session_manager_);

  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  profile_manager_observation_.Observe(profile_manager);

  UpdateStateIfProfileReady();
}

LoginScreenExtensionsLifetimeManager::~LoginScreenExtensionsLifetimeManager() {
  // `ShutDown()` and, optionally, `OnProfileManagerDestroying()` must have
  // already been called until this point.
  DCHECK(!profile_manager_observation_.IsObserving());
}

void LoginScreenExtensionsLifetimeManager::Shutdown() {
  // We need to do it here in addition to `OnProfileManagerDestroying()`, in
  // case the profile destruction happens before the profile manager's one.
  profile_manager_observation_.Reset();

  session_manager_observation_.Reset();
  extension_registry_observation_.Reset();

  // Cancel posted tasks and bound callbacks, in case there are any.
  weak_factory_.InvalidateWeakPtrs();
}

void LoginScreenExtensionsLifetimeManager::OnProfileAdded(Profile* profile) {
  if (profile == signin_original_profile_)
    UpdateState();
}

void LoginScreenExtensionsLifetimeManager::OnProfileManagerDestroying() {
  DCHECK(profile_manager_observation_.IsObserving());
  // We need to do this here in addition to `Shutdown()`, because the profile
  // manager destruction can start before the profile's one.
  profile_manager_observation_.Reset();
}

void LoginScreenExtensionsLifetimeManager::OnSessionStateChanged() {
  TRACE_EVENT0("login",
               "LoginScreenExtensionsLifetimeManager::OnSessionStateChanged");
  UpdateStateIfProfileReady();
}

void LoginScreenExtensionsLifetimeManager::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (extension->location() ==
          extensions::mojom::ManifestLocation::kExternalPolicyDownload &&
      !ShouldEnableLoginScreenPolicyExtensions()) {
    // The policy extensions should be disabled, however the extension got
    // loaded - due to the policy change or due to some internal reason in the
    // extensions subsystem. Therefore forcibly disable this extension.
    // Doing this in an asynchronous job, in order to avoid confusing other
    // observers of OnExtensionLoaded().
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&LoginScreenExtensionsLifetimeManager::DisableExtension,
                       weak_factory_.GetWeakPtr(), extension->id()));
  }
}

bool LoginScreenExtensionsLifetimeManager::
    ShouldEnableLoginScreenPolicyExtensions() const {
  // Note that extensions are intentionally allowed during the intermediate
  // transition states between the login screen and the user session, since the
  // user may still see some pieces of the login screen that rely on these
  // extensions.
  return session_manager_->session_state() !=
         session_manager::SessionState::ACTIVE;
}

extensions::ExtensionService*
LoginScreenExtensionsLifetimeManager::GetExtensionService() {
  // Note that we cannot cache the service pointer in our constructor, because
  // the service doesn't exist at that point.
  extensions::ExtensionService* service =
      extension_system_->extension_service();
  if (!service) {
    // Many unit tests skip initialization of profile services, including the
    // extension service. In production, the service should always be present at
    // this point (after the profile initialization completion).
    CHECK_IS_TEST();
  }
  return service;
}

void LoginScreenExtensionsLifetimeManager::UpdateStateIfProfileReady() {
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  if (!profile_manager->IsValidProfile(signin_original_profile_)) {
    // Wait until the profile is initialized - see `OnProfileAdded()`.
    return;
  }
  UpdateState();
}

void LoginScreenExtensionsLifetimeManager::UpdateState() {
  if (ShouldEnableLoginScreenPolicyExtensions())
    EnablePolicyExtensions();
  else
    DisablePolicyExtensions();
}

extensions::ExtensionIdList
LoginScreenExtensionsLifetimeManager::GetPolicyExtensionIds() const {
  const PrefService* const prefs = signin_original_profile_->GetPrefs();
  DCHECK_NE(prefs->GetAllPrefStoresInitializationStatus(),
            PrefService::INITIALIZATION_STATUS_WAITING);

  const PrefService::Preference* const pref =
      prefs->FindPreference(extensions::pref_names::kInstallForceList);
  if (!pref || !pref->IsManaged() ||
      pref->GetType() != base::Value::Type::DICT) {
    return {};
  }
  extensions::ExtensionIdList extension_ids;
  for (const auto item : pref->GetValue()->GetDict()) {
    extension_ids.push_back(item.first);
  }
  return extension_ids;
}

void LoginScreenExtensionsLifetimeManager::DisablePolicyExtensions() {
  extensions::ExtensionService* const extension_service = GetExtensionService();
  if (!extension_service)
    return;
  for (const extensions::ExtensionId& extension_id : GetPolicyExtensionIds()) {
    extension_service->DisableExtension(
        extension_id, extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
  }
}

void LoginScreenExtensionsLifetimeManager::EnablePolicyExtensions() {
  extensions::ExtensionService* const extension_service = GetExtensionService();
  if (!extension_service)
    return;
  // This reapplies the policy. For the extensions that were previously
  // disabled due to `DISABLE_BLOCKED_BY_POLICY`, this unsets this disable
  // reason and reenables the extension.
  extension_service->CheckManagementPolicy();

  // Make sure the event pages for the login-screen extensions are loaded
  // back, since they may need to do some operations on the Login/Lock Screen
  // but are likely to have missed the session state change notification.
  for (const extensions::ExtensionId& extension_id : GetPolicyExtensionIds())
    extensions_process_manager_->WakeEventPage(extension_id, base::DoNothing());
}

void LoginScreenExtensionsLifetimeManager::DisableExtension(
    const extensions::ExtensionId& extension_id) {
  extensions::ExtensionService* const extension_service = GetExtensionService();
  if (!extension_service)
    return;
  extension_service->DisableExtension(
      extension_id, extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
}

}  // namespace ash
