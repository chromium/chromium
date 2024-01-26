// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "google_apis/gaia/core_account_id.h"

class TokensLoadedCallbackRunner;

namespace signin_util {
class CookiesMover;
}

// Extracts an account from an existing profile and moves it to a new profile.
class DiceSignedInProfileCreator {
 public:
  // Creates a new profile or uses Guest profile if |use_guest_profile|, and
  // moves the account from source_profile to it.
  // The callback is called with the new profile or nullptr in case of failure.
  // The callback is never called synchronously.
  // If |local_profile_name| is not empty, it will be set as local name for the
  // new profile.
  // If |icon_index| is nullopt, a random icon will be selected.
  DiceSignedInProfileCreator(Profile* source_profile,
                             CoreAccountId account_id,
                             const std::u16string& local_profile_name,
                             std::optional<size_t> icon_index,
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
  // Called when the profile is initialized.
  void OnNewProfileInitialized(Profile* profile);

  // Called when cookies have been moved from `source_profile_` to
  // `new_profile`.
  void OnCookiesMoved(Profile* new_profile);

  void LoadNewProfileTokens(base::WeakPtr<Profile> new_profile);

  // Callback invoked once the token service is ready for the new profile.
  void OnNewProfileTokensLoaded(Profile* new_profile);

  const raw_ptr<Profile, DanglingUntriaged> source_profile_;
  const CoreAccountId account_id_;

  base::OnceCallback<void(Profile*)> callback_;
  std::unique_ptr<TokensLoadedCallbackRunner> tokens_loaded_callback_runner_;
  std::unique_ptr<signin_util::CookiesMover> cookies_mover_;

  base::WeakPtrFactory<DiceSignedInProfileCreator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_SIGNED_IN_PROFILE_CREATOR_H_
