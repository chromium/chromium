// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_OWNERSHIP_OWNER_KEY_LOADER_H_
#define CHROME_BROWSER_ASH_OWNERSHIP_OWNER_KEY_LOADER_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/ownership/owner_key_util.h"

class Profile;

namespace enterprise_management {
class PolicyData;
}

namespace ash {

class DeviceSettingsService;

BASE_DECLARE_FEATURE(kStoreOwnerKeyInPrivateSlot);
BASE_DECLARE_FEATURE(kMigrateOwnerKeyToPrivateSlot);

// A helper single-use class to load the owner key.
// Determines whether the current user is the owner or not.
// For the non-owner just loads the public owner key (which can be used to
// verify signature on the device policies).
// For the owner loads both public and private key or generates new ones if the
// previous ones were lost.
// For the first user that should become the owner generates a new key pair.
// All public methods might depend on the profile and therefore should be run on
// the UI thread.
class OwnerKeyLoader {
 public:
  using KeypairCallback = base::OnceCallback<void(
      scoped_refptr<ownership::PublicKey> public_key,
      scoped_refptr<ownership::PrivateKey> private_key)>;

  OwnerKeyLoader(Profile* profile,
                 DeviceSettingsService* device_settings_service,
                 scoped_refptr<ownership::OwnerKeyUtil> owner_key_util,
                 bool is_enterprise_managed,
                 KeypairCallback callback);
  OwnerKeyLoader(const OwnerKeyLoader&) = delete;
  auto operator=(const OwnerKeyLoader&) = delete;
  ~OwnerKeyLoader();

  // Starts the loading of the key(s). Can be called only once per instance of
  // the class.
  void Run();

  // Extracts the old owner key when it was replaced. Should be called after
  // Run() and after the `callback` (from the constructor) is called. Returns a
  // nullptr if the key was not replaced.
  // TODO(b/264397430): The method can be removed after the migration is done.
  crypto::ScopedSECKEYPrivateKey ExtractOldOwnerKey();

 private:
  void OnPublicKeyLoaded(scoped_refptr<ownership::PublicKey> public_key);
  void OnPrivateKeyLoaded(scoped_refptr<ownership::PrivateKey> private_key,
                          bool found_in_public_slot);
  void MaybeGenerateNewKey();
  void GenerateNewKey();
  void OnNewKeyGenerated(scoped_refptr<ownership::PublicKey> public_key,
                         scoped_refptr<ownership::PrivateKey> private_key);
  void MaybeRegenerateLostKey(
      const enterprise_management::PolicyData* policy_data);

  const raw_ptr<Profile> profile_;
  const raw_ptr<DeviceSettingsService> device_settings_service_;
  scoped_refptr<ownership::OwnerKeyUtil> owner_key_util_;
  const bool is_enterprise_managed_;
  scoped_refptr<ownership::PublicKey> public_key_;
  KeypairCallback callback_;
  int generate_attempt_counter_ = 0;
  // If not null, contains the old owner key that is currently being replaced by
  // the MigrateOwnerKeyToPrivateSlot experiment.
  // TODO(b/264397430): This can be removed after the migration is finished.
  crypto::ScopedSECKEYPrivateKey old_owner_key_;

  base::WeakPtrFactory<OwnerKeyLoader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_OWNERSHIP_OWNER_KEY_LOADER_H_
