// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/signin_profile_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/crx_file/id_util.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_system.h"

namespace ash {
namespace {

// This array contains a subset of the explicitly allowlisted extensions that
// are defined in extensions/common/api/_behavior_features.json. The extension
// is treated as risky if it has some UI elements which remain accessible
// after the signin was completed.
constexpr const char* kNonRiskyExtensionsIdsHashes[] = {
    "E24F1786D842E91E74C27929B0B3715A4689A473",  // Gnubby component extension
    "6F9E349A0561C78A0D3F41496FE521C5151C7F71",  // Gnubby app
    "06BE211D5F014BAB34BC22D9DDA09C63A81D828E",  // Chrome OS XKB
    "3F50C3A83839D9C76334BCE81CDEC06174F266AF",  // Virtual Keyboard
    "2F47B526FA71F44816618C41EC55E5EE9543FDCC",  // Braille Keyboard
    "86672C8D7A04E24EFB244BF96FE518C4C4809F73",  // Speech synthesis
    "1CF709D51B2B96CF79D00447300BD3BFBE401D21",  // Mobile activation
    "40FF1103292F40C34066E023B8BE8CAE18306EAE",  // Chromeos help
    "3C654B3B6682CA194E75AD044CEDE927675DDEE8",  // Easy unlock
    "75C7F4B720314B6CB1B5817CD86089DB95CD2461",  // ChromeVox
    "4D725C894DA4CF1F4D96C60F0D83BD745EB530CA",  // Switch Access
};

void WrapAsBrowsersCloseCallback(const base::RepeatingClosure& callback,
                                 const base::FilePath& path) {
  callback.Run();
}

SigninProfileHandler* g_instance = nullptr;

}  // namespace

SigninProfileHandler::SigninProfileHandler() {
  DCHECK(!g_instance);
  g_instance = this;
}

SigninProfileHandler::~SigninProfileHandler() {
  if (browsing_data_remover_)
    browsing_data_remover_->RemoveObserver(this);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

SigninProfileHandler* SigninProfileHandler::Get() {
  return g_instance;
}

void SigninProfileHandler::ProfileStartUp(Profile* profile) {
  // Initialize Chrome OS preferences like touch pad sensitivity. For the
  // preferences to work in the guest mode, the initialization has to be
  // done after |profile| is switched to the off-the-record profile (which
  // is actually GuestSessionProfile in the guest mode). See the
  // GetPrimaryOTRProfile() call above.
  profile->InitChromeOSPreferences();

  // Add observer so we can see when the first profile's session restore is
  // completed. After that, we won't need the default profile anymore.
  if (!ash::IsSigninBrowserContext(profile) &&
      user_manager::UserManager::Get()->IsLoggedInAsUserWithGaiaAccount() &&
      !user_manager::UserManager::Get()->IsLoggedInAsStub()) {
    auto* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile);
    if (login_manager)
      login_manager->AddObserver(this);
  }
}

void SigninProfileHandler::ClearSigninProfile(base::OnceClosure callback) {
  on_clear_callbacks_.push_back(std::move(callback));

  // Profile is already clearing.
  if (on_clear_callbacks_.size() > 1)
    return;

  if (!g_browser_process->profile_manager()) {
    OnSigninProfileCleared();
    return;
  }

  auto* signin_profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetSigninBrowserContext());
  if (!signin_profile) {
    OnSigninProfileCleared();
    return;
  }

  on_clear_profile_stage_finished_ = base::BarrierClosure(
      3, base::BindOnce(&SigninProfileHandler::OnSigninProfileCleared,
                        weak_factory_.GetWeakPtr()));
  DCHECK(!browsing_data_remover_);
  browsing_data_remover_ = signin_profile->GetBrowsingDataRemover();
  browsing_data_remover_->AddObserver(this);
  browsing_data_remover_->RemoveAndReply(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES, this);

  // Close the current session with SigninPartitionManager. This clears cached
  // data from the last-used sign-in StoragePartition.
  login::SigninPartitionManager::Factory::GetForBrowserContext(signin_profile)
      ->CloseCurrentSigninSession(on_clear_profile_stage_finished_);

  BrowserList::CloseAllBrowsersWithProfile(
      signin_profile,
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_success */,
      base::BindRepeating(
          &WrapAsBrowsersCloseCallback,
          on_clear_profile_stage_finished_) /* on_close_aborted */,
      true /* skip_beforeunload */);

  // Unload all extensions that could possibly leak the SigninProfile for
  // unauthorized usage.
  // TODO(crbug.com/40116250): This also can be fixed by restricting URLs
  //                                  or browser windows from opening.
  const std::set<std::string> allowed_ids_hashes(
      std::begin(kNonRiskyExtensionsIdsHashes),
      std::end(kNonRiskyExtensionsIdsHashes));
  auto* component_loader = extensions::ExtensionSystem::Get(signin_profile)
                               ->extension_service()
                               ->component_loader();
  const std::vector<std::string> loaded_extensions =
      component_loader->GetRegisteredComponentExtensionsIds();
  for (const auto& el : loaded_extensions) {
    const std::string hex_hash = crx_file::id_util::HashedIdInHex(el);
    if (!allowed_ids_hashes.count(hex_hash))
      component_loader->Remove(el);
  }
}

void SigninProfileHandler::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  if (state == OAuth2LoginManager::SESSION_RESTORE_DONE ||
      state == OAuth2LoginManager::SESSION_RESTORE_FAILED ||
      state == OAuth2LoginManager::SESSION_RESTORE_CONNECTION_FAILED) {
    auto* login_manager =
        OAuth2LoginManagerFactory::GetInstance()->GetForProfile(user_profile);
    login_manager->RemoveObserver(this);
    ClearSigninProfile(base::OnceClosure());
  }
}

void SigninProfileHandler::OnBrowsingDataRemoverDone(
    uint64_t failed_data_types) {
  DCHECK(browsing_data_remover_);
  browsing_data_remover_->RemoveObserver(this);
  browsing_data_remover_ = nullptr;

  on_clear_profile_stage_finished_.Run();
}

void SigninProfileHandler::OnSigninProfileCleared() {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(on_clear_callbacks_);
  for (auto& callback : callbacks) {
    if (!callback.is_null())
      std::move(callback).Run();
  }
}

}  // namespace ash
