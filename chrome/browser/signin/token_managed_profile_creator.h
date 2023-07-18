// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_TOKEN_MANAGED_PROFILE_CREATOR_H_
#define CHROME_BROWSER_SIGNIN_TOKEN_MANAGED_PROFILE_CREATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace signin_util {
class CookiesMover;
}

// Extracts an account from an existing profile and moves it to a new profile.
class TokenManagedProfileCreator : public ProfileAttributesStorageObserver {
 public:
  // Creates a new profile or uses Guest profile if |use_guest_profile|, and
  // moves the account from source_profile to it.
  // The callback is called with the new profile or nullptr in case of failure.
  // The callback is never called synchronously.
  // If |local_profile_name| is not empty, it will be set as local name for the
  // new profile.
  // If |icon_index| is nullopt, a random icon will be selected.
  TokenManagedProfileCreator(
      Profile* source_profile,
      const std::string& id,
      const std::string& enrollment_token,
      const std::u16string& local_profile_name,
      base::OnceCallback<void(base::WeakPtr<Profile>)> callback);

  // Uses this version when the profile already exists at `target_profile_path`
  // but may not be loaded in memory. The profile is loaded if necessary, and
  // the account is moved.
  TokenManagedProfileCreator(
      Profile* source_profile,
      const base::FilePath& target_profile_path,
      base::OnceCallback<void(base::WeakPtr<Profile>)> callback);

  ~TokenManagedProfileCreator() override;

  TokenManagedProfileCreator(const TokenManagedProfileCreator&) = delete;
  TokenManagedProfileCreator& operator=(const TokenManagedProfileCreator&) =
      delete;

  // ProfileAttributesStorageObserver implementation
  void OnProfileAdded(const base::FilePath& profile_path) override;

 private:
  // Called when the profile is created.
  void OnNewProfileCreated(Profile* new_profile);

  // Called when the profile is initialized.
  void OnNewProfileInitialized(Profile* new_profile);

  // Callback invoked once the token service is ready for the new profile.
  void OnNewProfileTokensLoaded(Profile* new_profile);

  base::raw_ptr<Profile> source_profile_;
  const std::string id_;
  const std::string enrollment_token_;
  base::FilePath expected_profile_path_;
  base::OnceCallback<void(base::WeakPtr<Profile>)> callback_;
  std::unique_ptr<signin_util::CookiesMover> cookies_mover_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_observation_{this};
  base::WeakPtrFactory<TokenManagedProfileCreator> weak_pointer_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_TOKEN_MANAGED_PROFILE_CREATOR_H_
