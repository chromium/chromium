// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace policy {

class CloudPolicyManager;

// Provides invalidations to user policy. Implemented as a
// KeyedService to allow profile-based lifetime management.
class UserCloudPolicyInvalidator : public CloudPolicyInvalidator,
                                   public KeyedService,
                                   public ProfileObserver {
 public:
  // |profile| is profile associated with the invalidator. It is used to get
  // a reference to the profile's invalidation service. Both the profile and
  // invalidation service must remain valid until Shutdown is called.
  // |policy_manager| is the policy manager for the user policy and must remain
  // valid until Shutdown is called.
  UserCloudPolicyInvalidator(Profile* profile,
                             CloudPolicyManager* policy_manager);
  ~UserCloudPolicyInvalidator() override;
  UserCloudPolicyInvalidator(const UserCloudPolicyInvalidator&) = delete;
  UserCloudPolicyInvalidator& operator=(const UserCloudPolicyInvalidator&) =
      delete;

  // KeyedService:
  void Shutdown() override;

  // ProfileObserver implementation:
  void OnProfileInitializationComplete(Profile* profile) override;

 private:
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_
