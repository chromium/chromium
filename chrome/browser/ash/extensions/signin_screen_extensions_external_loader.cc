// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/signin_screen_extensions_external_loader.h"

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
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

SigninScreenExtensionsExternalLoader::SigninScreenExtensionsExternalLoader(
    Profile* profile)
    : profile_(profile),
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
  DCHECK(ash::ProfileHelper::IsSigninProfile(profile));
}

void SigninScreenExtensionsExternalLoader::StartLoading() {
  PrefService* const prefs = profile_->GetPrefs();
  if (prefs->GetAllPrefStoresInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    prefs->AddPrefInitObserver(base::BindOnce(
        &SigninScreenExtensionsExternalLoader::OnPrefsInitialized,
        weak_factory_.GetWeakPtr()));
    return;
  }
  SubscribeAndInitializeFromPrefs();
}

void SigninScreenExtensionsExternalLoader::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  if (initial_load_finished_) {
    OnUpdated(prefs.Clone());
    return;
  }
  initial_load_finished_ = true;
  LoadFinished(prefs.Clone());
}

bool SigninScreenExtensionsExternalLoader::IsRollbackAllowed() const {
  return true;
}

SigninScreenExtensionsExternalLoader::~SigninScreenExtensionsExternalLoader() =
    default;

void SigninScreenExtensionsExternalLoader::OnPrefsInitialized(
    bool /*success*/) {
  SubscribeAndInitializeFromPrefs();
}

void SigninScreenExtensionsExternalLoader::SubscribeAndInitializeFromPrefs() {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      extensions::pref_names::kInstallForceList,
      base::BindRepeating(
          &SigninScreenExtensionsExternalLoader::UpdateStateFromPrefs,
          base::Unretained(this)));

  UpdateStateFromPrefs();
}

void SigninScreenExtensionsExternalLoader::UpdateStateFromPrefs() {
  auto force_installed_extensions =
      GetForceInstalledExtensionsFromPrefs(profile_->GetPrefs());
  external_cache_.UpdateExtensionsList(std::move(force_installed_extensions));
}

}  // namespace chromeos
