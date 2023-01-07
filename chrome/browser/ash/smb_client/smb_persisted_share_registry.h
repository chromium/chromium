// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_

#include <vector>

#include "chrome/browser/ash/smb_client/smb_share_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {
namespace smb_client {

class SmbUrl;

// Handles saving of SMB shares in the user's Profile.
class SmbPersistedShareRegistry {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit SmbPersistedShareRegistry(Profile* profile);

  // Disallow copy/assign.
  SmbPersistedShareRegistry() = delete;
  SmbPersistedShareRegistry(const SmbPersistedShareRegistry&) = delete;
  SmbPersistedShareRegistry& operator=(const SmbPersistedShareRegistry&) =
      delete;

  // Save |share| in the user's profile. If a saved share already exists with
  // the url share.share_url(), that saved share will be overwritten.
  void Save(const SmbShareInfo& share);

  // Delete the saved share with the URL |share_url|.
  void Delete(const SmbUrl& share_url);

  // Return the saved share with URL |share_url|, or the empty Optional<> if no
  // share is found.
  absl::optional<SmbShareInfo> Get(const SmbUrl& share_url) const;

  // Return a list of all saved shares.
  std::vector<SmbShareInfo> GetAll() const;

 private:
  Profile* const profile_;
};

}  // namespace smb_client
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_PERSISTED_SHARE_REGISTRY_H_
