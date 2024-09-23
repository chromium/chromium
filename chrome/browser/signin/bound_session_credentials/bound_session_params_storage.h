// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_

#include <memory>
#include <optional>
#include <string_view>

#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"

class GURL;
class Profile;
class PrefRegistrySimple;
class PrefService;

// Stores bound session parameters.
//
// Depending on the profile type, either
// - stores the parameters in memory if a profile is off-the-record, or
// - stores the parameters on disk, in user prefs, otherwise
//
// Session params are keyed by a (site, session_id) pair, which should be
// uniquely identifying.
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

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Saves `params` to storage. Overwrites existing params with the same
  // (site, session_id) key.
  // `params` are verified before being saved.
  // Returns whether the new parameters were saved. In case of a failure, keeps
  // the existing value intact.
  [[nodiscard]] virtual bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) = 0;

  // Patches up storage entries that have one of the known issues:
  // - https://crbug.com/349411334 (added 2024/06)
  // - https://crbug.com/325441004 (added 2024/07)
  // After update is complete, prunes all invalid entries, and returns
  // parameters for all valid stored sessions.
  virtual std::vector<bound_session_credentials::BoundSessionParams>
  ReadAllParamsAndCleanStorageIfNecessary() = 0;

  // Removes params identified by (site, session_id) from the storage.
  // Returns true if an entry was removed or false otherwise.
  virtual bool ClearParams(const GURL& site, std::string_view session_id) = 0;

  // Completely wipes the storage.
  virtual void ClearAllParams() = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_PARAMS_STORAGE_H_
