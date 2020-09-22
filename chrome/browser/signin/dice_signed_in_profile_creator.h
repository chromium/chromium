// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/core_account_id.h"

class TokensLoadedCallbackRunner;

// Extracts an account from an existing profile and moves it to a new profile.
class DiceSignedInProfileCreator {
 public:
  // Creates a new profile and moves the account from source_profile to the new
  // profile. The callback is called with the new profile or nullptr in case of
  // failure. The callback is never called synchronously.
  // If |local_profile_name| is not empty, it will be set as local name for the
  // new profile.
  // If |icon_index| is nullopt, a random icon will be selected.
  DiceSignedInProfileCreator(Profile* source_profile,
                             CoreAccountId account_id,
                             const base::string16& local_profile_name,
                             base::Optional<size_t> icon_index,
                             base::OnceCallback<void(Profile*)> callback);

  // Uses this version when the profile already exists at `target_profile_path`
  // but may not be loaded in memory. The profile is loaded if necessary, and
  // the account is moved.
  DiceSignedInProfileCreator(Profile* source_profile,
                             CoreAccountId account_id,
                             const base::FilePath& target_profile_path,
                             base::OnceCallback<void(Profile*)> callback);

  ~DiceSignedInProfileCreator();

  DiceSignedInProfileCreator(const DiceSignedInProfileCreator&) = delete;
  DiceSignedInProfileCreator& operator=(const DiceSignedInProfileCreator&) =
      delete;

 private:
  // Callback invoked once a profile is created, so we can transfer the
  // credentials.
  void OnNewProfileCreated(Profile* new_profile, Profile::CreateStatus status);

  // Called when the profile is initialized.
  void OnNewProfileInitialized(Profile* new_profile);

  // Callback invoked once the token service is ready for the new profile.
  void OnNewProfileTokensLoaded(Profile* new_profile);

  Profile* const source_profile_;
  const CoreAccountId account_id_;

  base::OnceCallback<void(Profile*)> callback_;
  std::unique_ptr<TokensLoadedCallbackRunner> tokens_loaded_callback_runner_;

  base::WeakPtrFactory<DiceSignedInProfileCreator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
