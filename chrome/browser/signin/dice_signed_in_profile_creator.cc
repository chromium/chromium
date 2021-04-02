// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_signed_in_profile_creator.h"

#include <string>

#include "base/check.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

const void* const
    DiceSignedInProfileCreator::kGuestSigninTokenTransferredUserDataKey =
        &DiceSignedInProfileCreator::kGuestSigninTokenTransferredUserDataKey;

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
    std::move(callback_).Run(profile_);
  }

  void OnIdentityManagerShutdown(signin::IdentityManager* manager) override {
    scoped_identity_manager_observer_.Reset();
    std::move(callback_).Run(nullptr);
  }

  Profile* profile_;
  signin::IdentityManager* identity_manager_;
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
  scoped_identity_manager_observer_.Observe(identity_manager_);
}

DiceSignedInProfileCreator::DiceSignedInProfileCreator(
    Profile* source_profile,
    CoreAccountId account_id,
    const std::u16string& local_profile_name,
    base::Optional<size_t> icon_index,
    bool use_guest_profile,
    base::OnceCallback<void(Profile*)> callback)
    : source_profile_(source_profile),
      account_id_(account_id),
      callback_(std::move(callback)) {
  // Passing the sign-in token to an ephemeral Guest profile is part of the
  // experiment to surface a Guest mode link in the DiceWebSigninIntercept
  // and is only used to sign in to the web through account consistency and
  // does NOT enable sync or any other browser level functionality.
  // TODO(https://crbug.com/1125474): Revise the comment after ephemeral Guest
  // profiles are finalized.
  if (use_guest_profile) {
    DCHECK(Profile::IsEphemeralGuestProfileEnabled());
    // Make sure the callback is not called synchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProfileManager::CreateProfileAsync,
                       base::Unretained(g_browser_process->profile_manager()),
                       ProfileManager::GetGuestProfilePath(),
                       base::BindRepeating(
                           &DiceSignedInProfileCreator::OnNewProfileCreated,
                           weak_pointer_factory_.GetWeakPtr())));
  } else {
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    if (!icon_index.has_value())
      icon_index = storage.ChooseAvatarIconIndexForNewProfile();
    std::u16string name = local_profile_name.empty()
                              ? storage.ChooseNameForNewProfile(*icon_index)
                              : local_profile_name;
    ProfileManager::CreateMultiProfileAsync(
        name, *icon_index,
        base::BindRepeating(&DiceSignedInProfileCreator::OnNewProfileCreated,
                            weak_pointer_factory_.GetWeakPtr()));
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(&ProfileManager::LoadProfileByPath),
          base::Unretained(g_browser_process->profile_manager()),
          target_profile_path, /*incognito=*/false,
          base::BindOnce(&DiceSignedInProfileCreator::OnNewProfileInitialized,
                         weak_pointer_factory_.GetWeakPtr())));
}

DiceSignedInProfileCreator::~DiceSignedInProfileCreator() = default;

void DiceSignedInProfileCreator::OnNewProfileCreated(
    Profile* new_profile,
    Profile::CreateStatus status) {
  switch (status) {
    case Profile::CREATE_STATUS_CREATED:
      // Ignore this, wait for profile to be initialized.
      return;
    case Profile::CREATE_STATUS_INITIALIZED:
      OnNewProfileInitialized(new_profile);
      return;
    case Profile::CREATE_STATUS_REMOTE_FAIL:
    case Profile::CREATE_STATUS_CANCELED:
    case Profile::MAX_CREATE_STATUS:
      NOTREACHED() << "Invalid profile creation status";
      FALLTHROUGH;
    case Profile::CREATE_STATUS_LOCAL_FAIL:
      NOTREACHED() << "Error creating new profile";
      if (callback_)
        std::move(callback_).Run(nullptr);
      return;
  }
}

void DiceSignedInProfileCreator::OnNewProfileInitialized(Profile* new_profile) {
  if (!new_profile) {
    if (callback_)
      std::move(callback_).Run(nullptr);
    return;
  }

  DCHECK(!tokens_loaded_callback_runner_);
  // base::Unretained is fine because the runner is owned by this.
  auto tokens_loaded_callback_runner =
      TokensLoadedCallbackRunner::RunWhenLoaded(
          new_profile,
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
  if (new_profile->IsEphemeralGuestProfile())
    GuestSigninTokenTransferredUserData::Set(new_profile);
  if (callback_)
    std::move(callback_).Run(new_profile);
}
