// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_OBSOLETE_PROFILE_GARBAGE_COLLECTOR_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_OBSOLETE_PROFILE_GARBAGE_COLLECTOR_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"

class Profile;
class ProfileManager;

namespace unexportable_keys {

class UnexportableKeyService;

class UnexportableKeyObsoleteProfileGarbageCollector
    : public ProfileManagerObserver {
 public:
  explicit UnexportableKeyObsoleteProfileGarbageCollector(
      ProfileManager* profile_manager);
  ~UnexportableKeyObsoleteProfileGarbageCollector() override;

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  void StartGarbageCollection();
  void OnGetAllSigningKeysForGarbageCollection(
      ServiceErrorOr<std::vector<UnexportableKeyId>> keys_or_error);

  ProfileManager* profile_manager() {
    return profile_manager_observation_.GetSource();
  }

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  std::unique_ptr<UnexportableKeyService> user_data_dir_service_;
  base::WeakPtrFactory<UnexportableKeyObsoleteProfileGarbageCollector>
      weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_OBSOLETE_PROFILE_GARBAGE_COLLECTOR_H_
