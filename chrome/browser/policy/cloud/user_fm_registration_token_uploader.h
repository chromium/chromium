// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace policy {

class CloudPolicyManager;
class FmRegistrationTokenUploader;

// Uploads Firebase registration token to the backend server. Implemented as a
// KeyedService to allow profile-based lifetime management.
class UserFmRegistrationTokenUploader : public KeyedService,
                                        public ProfileObserver {
 public:
  // `profile` is profile associated with the invalidator. It is used to get
  // a reference to the profile's invalidation service. Both the profile and
  // invalidation service must remain valid until Shutdown is called.
  // `policy_manager` is the policy manager for the user policy and must remain
  // valid until Shutdown is called.
  UserFmRegistrationTokenUploader(Profile* profile,
                                  CloudPolicyManager* policy_manager);
  ~UserFmRegistrationTokenUploader() override;
  UserFmRegistrationTokenUploader(const UserFmRegistrationTokenUploader&) =
      delete;
  UserFmRegistrationTokenUploader& operator=(
      const UserFmRegistrationTokenUploader&) = delete;

  // KeyedService:
  void Shutdown() override;

  // ProfileObserver implementation:
  void OnProfileInitializationComplete(Profile* profile) override;

 private:
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  raw_ptr<CloudPolicyManager> policy_manager_ = nullptr;

  std::unique_ptr<FmRegistrationTokenUploader> uploader_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_FM_REGISTRATION_TOKEN_UPLOADER_H_
