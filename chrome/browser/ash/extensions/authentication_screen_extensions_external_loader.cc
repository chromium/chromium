// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/authentication_screen_extensions_external_loader.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "extensions/browser/pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

base::Value::Dict GetForceInstalledExtensionsFromPrefs(
    const PrefService* prefs) {
  const PrefService::Preference* const login_screen_extensions_pref =
      prefs->FindPreference(extensions::pref_names::kInstallForceList);
  CHECK(login_screen_extensions_pref);
  if (!login_screen_extensions_pref->IsManaged() &&
      !login_screen_extensions_pref->IsDefaultValue()) {
    // Ignore untrusted values - only the policy-specified setting is respected.
    // (This branch could be triggered if, for example, an attacker modified the
    // Local State file trying to inject some extensions into the Login Screen.)
    LOG(WARNING) << "Ignoring untrusted value of the "
                 << extensions::pref_names::kInstallForceList << " pref";
    return base::Value::Dict();
  }
  const base::Value* login_screen_extensions_pref_value =
      login_screen_extensions_pref->GetValue();
  DCHECK(login_screen_extensions_pref_value->is_dict());
  return login_screen_extensions_pref_value->GetDict().Clone();
}

// Whether extensions should be loaded on the lock screen instead of the sign-in
// profile.
bool IsLockScreenTakingOver() {
  const auto session_state =
      session_manager::SessionManager::Get()->session_state();
  return chromeos::features::IsLockScreenBadgeAuthEnabled() &&
         session_state == session_manager::SessionState::LOCKED &&
         ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext();
}
}  // namespace

AuthenticationScreenExtensionsExternalLoader::
    AuthenticationScreenExtensionsExternalLoader(Profile* profile)
    : profile_(profile),
      external_cache_(base::PathService::CheckedGet(
                          ash::IsLockScreenBrowserContext(profile)
                              ? ash::DIR_LOCK_PROFILE_EXTENSIONS
                              : ash::DIR_SIGNIN_PROFILE_EXTENSIONS),
                      g_browser_process->shared_url_loader_factory(),
                      base::ThreadPool::CreateSequencedTaskRunner(
                          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
                      this,
                      /*always_check_updates=*/true,
                      /*wait_for_cache_initialization=*/false,
                      /*allow_scheduled_updates=*/false) {
  DCHECK(ash::IsSigninBrowserContext(profile) ||
         (chromeos::features::IsLockScreenBadgeAuthEnabled() &&
          ash::IsLockScreenBrowserContext(profile)));
  if (chromeos::features::IsLockScreenBadgeAuthEnabled()) {
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
  }
}

void AuthenticationScreenExtensionsExternalLoader::StartLoading() {
  PrefService* const prefs = profile_->GetPrefs();
  if (prefs->GetAllPrefStoresInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    prefs->AddPrefInitObserver(base::BindOnce(
        &AuthenticationScreenExtensionsExternalLoader::OnPrefsInitialized,
        weak_factory_.GetWeakPtr()));
    return;
  }
  SubscribeAndInitializeFromPrefs();
}

void AuthenticationScreenExtensionsExternalLoader::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  if (initial_load_finished_) {
    OnUpdated(prefs.Clone());
    return;
  }
  initial_load_finished_ = true;
  LoadFinished(prefs.Clone());
}

bool AuthenticationScreenExtensionsExternalLoader::IsRollbackAllowed() const {
  return true;
}

void AuthenticationScreenExtensionsExternalLoader::OnSessionStateChanged() {
  UpdateStateFromPrefs();
}

AuthenticationScreenExtensionsExternalLoader::
    ~AuthenticationScreenExtensionsExternalLoader() = default;

void AuthenticationScreenExtensionsExternalLoader::OnPrefsInitialized(
    bool /*success*/) {
  SubscribeAndInitializeFromPrefs();
}

void AuthenticationScreenExtensionsExternalLoader::
    SubscribeAndInitializeFromPrefs() {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      extensions::pref_names::kInstallForceList,
      base::BindRepeating(
          &AuthenticationScreenExtensionsExternalLoader::UpdateStateFromPrefs,
          base::Unretained(this)));

  UpdateStateFromPrefs();
}

void AuthenticationScreenExtensionsExternalLoader::UpdateStateFromPrefs() {
  bool is_lock_screen_taking_over = IsLockScreenTakingOver();
  bool should_load_extensions;
  if (ash::IsSigninBrowserContext(profile_)) {
    should_load_extensions = !is_lock_screen_taking_over;
  } else {
    DCHECK(ash::IsLockScreenBrowserContext(profile_));
    should_load_extensions = is_lock_screen_taking_over;
  }

  auto force_installed_extensions =
      should_load_extensions
          ? GetForceInstalledExtensionsFromPrefs(profile_->GetPrefs())
          : base::Value::Dict();
  // TODO(crbug.com/447591120): On the lock profile, only load badge based auth
  // extensions.
  external_cache_.UpdateExtensionsList(std::move(force_installed_extensions));
}

}  // namespace chromeos
