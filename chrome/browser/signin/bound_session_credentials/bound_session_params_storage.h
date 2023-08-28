// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_

#include <memory>

#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Stores bound session parameters.
//
// Depending on the profile type, either
// - stores the parameters in memory if a profile is off-the-record, or
// - stores the parameters on disk, in user prefs, otherwise
//
// Currently, supports only a single simultaneous session.
class BoundSessionParamsStorage {
 public:
  BoundSessionParamsStorage() = default;

  BoundSessionParamsStorage(const BoundSessionParamsStorage&) = delete;
  BoundSessionParamsStorage& operator=(const BoundSessionParamsStorage&) =
      delete;

  virtual ~BoundSessionParamsStorage() = default;

  // Creates a new storage instance for `profile`.
  static std::unique_ptr<BoundSessionParamsStorage> CreateForProfile(
      Profile& profile);

  // Allows tests to create a prefs-backed storage without creating a testing
  // profile.
  static std::unique_ptr<BoundSessionParamsStorage>
  CreatePrefsStorageForTesting(PrefService& pref_service);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool AreParamsValid(
      const bound_session_credentials::BoundSessionParams&
          bound_session_params);

  // Saves `params` to storage. Overwrites existing params if any. `params` are
  // verified before being saved.
  // Returns whether the new parameters were saved. In case of a failure, keeps
  // the existing value intact.
  [[nodiscard]] virtual bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) = 0;

  // Returns currently stored parameters if any.
  virtual absl::optional<bound_session_credentials::BoundSessionParams>
  ReadParams() const = 0;

  // Cleans the storage.
  virtual void ClearParams() = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_
