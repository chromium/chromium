// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_signed_in_profile_creator.h"

#include <string>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

// Waits until the tokens are loaded and calls the callback. The callback is
// called immediately if the tokens are already loaded, and called with nullptr
// if the profile is destroyed before the tokens are loaded.
class TokensLoadedCallbackRunner : public signin::IdentityManager::Observer {
 public:
  ~TokensLoadedCallbackRunner() override = default;
  TokensLoadedCallbackRunner(const TokensLoadedCallbackRunner&) = delete;
  TokensLoadedCallbackRunner& operator=(const TokensLoadedCallbackRunner&) =
      delete;

  // Runs the callback when the tokens are loaded. If tokens are already loaded
  // the callback is called synchronously and this returns nullptr.
  static std::unique_ptr<TokensLoadedCallbackRunner> RunWhenLoaded(
      Profile* profile,
      base::OnceCallback<void(Profile*)> callback);

 private:
  TokensLoadedCallbackRunner(Profile* profile,
                             base::OnceCallback<void(Profile*)> callback);

  // signin::IdentityManager::Observer implementation:
  void OnRefreshTokensLoaded() override {
    scoped_identity_manager_observer_.Reset();
    std::move(callback_).Run(profile_.get());
  }

  void OnIdentityManagerShutdown(signin::IdentityManager* manager) override {
    scoped_identity_manager_observer_.Reset();
    std::move(callback_).Run(nullptr);
  }

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observer_{this};
  base::OnceCallback<void(Profile*)> callback_;
};

// static
std::unique_ptr<TokensLoadedCallbackRunner>
TokensLoadedCallbackRunner::RunWhenLoaded(
    Profile* profile,
    base::OnceCallback<void(Profile*)> callback) {
  if (IdentityManagerFactory::GetForProfile(profile)
          ->AreRefreshTokensLoaded()) {
    std::move(callback).Run(profile);
    return nullptr;
  }

  return base::WrapUnique(
      new TokensLoadedCallbackRunner(profile, std::move(callback)));
}

TokensLoadedCallbackRunner::TokensLoadedCallbackRunner(
    Profile* profile,
    base::OnceCallback<void(Profile*)> callback)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      callback_(std::move(callback)) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(callback_);
  DCHECK(!identity_manager_->AreRefreshTokensLoaded());
  scoped_identity_manager_observer_.Observe(identity_manager_.get());
}

DiceSignedInProfileCreator::DiceSignedInProfileCreator(
    Profile* source_profile,
    CoreAccountId account_id,
    const std::u16string& local_profile_name,
    absl::optional<size_t> icon_index,
    bool use_guest_profile,
    base::OnceCallback<void(Profile*)> callback)
    : source_profile_(source_profile),
      account_id_(account_id),
      callback_(std::move(callback)) {
  auto initialized_callback =
      base::BindOnce(&DiceSignedInProfileCreator::OnNewProfileInitialized,
                     weak_pointer_factory_.GetWeakPtr());

  // Passing the sign-in token to an ephemeral Guest profile is part of the
  // experiment to surface a Guest mode link in the DiceWebSigninIntercept
  // and is only used to sign in to the web through account consistency and
  // does NOT enable sync or any other browser level functionality.
  // TODO(https://crbug.com/1225171): Revise the comment after Guest mode plans
  // are finalized.
  if (use_guest_profile) {
    // TODO(https://crbug.com/1225171): Re-enabled if ephemeral based Guest mode
    // is added. Remove the code otherwise.
    NOTREACHED();

    // Make sure the callback is not called synchronously.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProfileManager::CreateProfileAsync,
                       base::Unretained(g_browser_process->profile_manager()),
                       ProfileManager::GetGuestProfilePath(),
                       std::move(initialized_callback), base::DoNothing()));
  } else {
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    if (!icon_index.has_value())
      icon_index = storage.ChooseAvatarIconIndexForNewProfile();
    std::u16string name = local_profile_name.empty()
                              ? storage.ChooseNameForNewProfile(*icon_index)
                              : local_profile_name;
    ProfileManager::CreateMultiProfileAsync(name, *icon_index,
                                            /*is_hidden=*/false,
                                            std::move(initialized_callback));
  }
}

DiceSignedInProfileCreator::DiceSignedInProfileCreator(
    Profile* source_profile,
    CoreAccountId account_id,
    const base::FilePath& target_profile_path,
    base::OnceCallback<void(Profile*)> callback)
    : source_profile_(source_profile),
      account_id_(account_id),
      callback_(std::move(callback)) {
  // Make sure the callback is not called synchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ProfileManager::LoadProfileByPath),
          base::Unretained(g_browser_process->profile_manager()),
          target_profile_path, /*incognito=*/false,
          base::BindOnce(&DiceSignedInProfileCreator::OnNewProfileInitialized,
                         weak_pointer_factory_.GetWeakPtr())));
}

DiceSignedInProfileCreator::~DiceSignedInProfileCreator() = default;

void DiceSignedInProfileCreator::OnNewProfileInitialized(Profile* new_profile) {
  if (!new_profile) {
    NOTREACHED() << "Error creating new profile";
    if (callback_)
      std::move(callback_).Run(nullptr);
    return;
  }

  cookies_mover_ = std::make_unique<signin_util::CookiesMover>(
      source_profile_->GetWeakPtr(), new_profile->GetWeakPtr(),
      base::BindOnce(&DiceSignedInProfileCreator::LoadNewProfileTokens,
                     weak_pointer_factory_.GetWeakPtr(),
                     new_profile->GetWeakPtr()));
  cookies_mover_->StartMovingCookies();
}

void DiceSignedInProfileCreator::LoadNewProfileTokens(
    base::WeakPtr<Profile> new_profile) {
  if (new_profile.WasInvalidated()) {
    if (callback_) {
      std::move(callback_).Run(nullptr);
    }
    return;
  }
  DCHECK(!tokens_loaded_callback_runner_);
  // base::Unretained is fine because the runner is owned by this.
  auto tokens_loaded_callback_runner =
      TokensLoadedCallbackRunner::RunWhenLoaded(
          new_profile.get(),
          base::BindOnce(&DiceSignedInProfileCreator::OnNewProfileTokensLoaded,
                         base::Unretained(this)));
  // If the callback was called synchronously, |this| may have been deleted.
  if (tokens_loaded_callback_runner) {
    tokens_loaded_callback_runner_ = std::move(tokens_loaded_callback_runner);
  }
}

void DiceSignedInProfileCreator::OnNewProfileTokensLoaded(
    Profile* new_profile) {
  tokens_loaded_callback_runner_.reset();
  if (!new_profile) {
    if (callback_)
      std::move(callback_).Run(nullptr);
    return;
  }

  auto* accounts_mutator =
      IdentityManagerFactory::GetForProfile(source_profile_)
          ->GetAccountsMutator();
  auto* new_profile_accounts_mutator =
      IdentityManagerFactory::GetForProfile(new_profile)->GetAccountsMutator();
  accounts_mutator->MoveAccount(new_profile_accounts_mutator, account_id_);
  if (callback_)
    std::move(callback_).Run(new_profile);
}
