// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/authentication_screen_extensions_external_loader.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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

// Prod Identity Card Connector extension ID.
constexpr char kIdentityCardConnectorExtensionId[] =
    "agicampiiinkgdgceoknnjecpoamgigi";

// Holds the ID to check instead of the prod ID when running tests.
const char* g_test_extension_id_override = nullptr;

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

// Returns true if the Identity Card Connector extension (or a test override)
// is force-installed via policy.
bool IsBadgeBasedAuthenticationEnabled(
    const base::Value::Dict& force_installed_extensions) {
  std::string target_id;
  if (g_test_extension_id_override) {
    CHECK_IS_TEST();
    target_id = g_test_extension_id_override;
  } else {
    target_id = kIdentityCardConnectorExtensionId;
  }

  return force_installed_extensions.Find(target_id) != nullptr;
}

// Whether extensions should be loaded on the lock screen instead of the sign-in
// profile.
bool IsLockScreenTakingOver(
    const base::Value::Dict& force_installed_extensions) {
  const session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  return chromeos::features::IsLockScreenBadgeAuthEnabled() &&
         session_state == session_manager::SessionState::LOCKED &&
         ash::BrowserContextHelper::Get()->GetLockScreenBrowserContext() &&
         IsBadgeBasedAuthenticationEnabled(force_installed_extensions);
}

}  // namespace

// static
void AuthenticationScreenExtensionsExternalLoader::
    SetTestBadgeAuthExtensionIdForTesting(const char* id) {
  g_test_extension_id_override = id;
}

AuthenticationScreenExtensionsExternalLoader::
    AuthenticationScreenExtensionsExternalLoader(Profile* profile)
    : profile_(profile),
      // TODO(crbug.com/447583060): Separate cache for lock screen.
      external_cache_(
          base::PathService::CheckedGet(ash::DIR_SIGNIN_PROFILE_EXTENSIONS),
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

    ProfileManager* const profile_manager =
        g_browser_process->profile_manager();
    DCHECK(profile_manager);
    profile_manager_observation_.Observe(profile_manager);
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

void AuthenticationScreenExtensionsExternalLoader::OnProfileAdded(
    Profile* profile) {
  if (ash::IsSigninBrowserContext(profile) ||
      ash::IsLockScreenBrowserContext(profile)) {
    UpdateStateFromPrefs();
  }
}

void AuthenticationScreenExtensionsExternalLoader::
    OnProfileManagerDestroying() {
  DCHECK(profile_manager_observation_.IsObserving());
  // We need to do this here in addition to `Shutdown()`, because the profile
  // manager destruction can start before the profile's one.
  profile_manager_observation_.Reset();
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
  auto force_installed_extensions =
      GetForceInstalledExtensionsFromPrefs(profile_->GetPrefs());
  const bool is_lock_screen_taking_over =
      IsLockScreenTakingOver(force_installed_extensions);
  bool should_load_extensions;

  if (ash::IsSigninBrowserContext(profile_)) {
    should_load_extensions = !is_lock_screen_taking_over;
  } else {
    DCHECK(ash::IsLockScreenBrowserContext(profile_));
    should_load_extensions = is_lock_screen_taking_over;
  }

  external_cache_.UpdateExtensionsList(
      should_load_extensions ? std::move(force_installed_extensions)
                             : base::Value::Dict());
}

}  // namespace chromeos
