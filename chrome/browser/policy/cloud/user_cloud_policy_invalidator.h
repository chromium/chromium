// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_

#include "base/macros.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace policy {

class CloudPolicyManager;

// Provides invalidations to user policy. Implemented as a
// KeyedService to allow profile-based lifetime management.
class UserCloudPolicyInvalidator : public CloudPolicyInvalidator,
                                   public KeyedService,
                                   public content::NotificationObserver {
 public:
  // |profile| is profile associated with the invalidator. It is used to get
  // a reference to the profile's invalidation service. Both the profile and
  // invalidation service must remain valid until Shutdown is called.
  // |policy_manager| is the policy manager for the user policy and must remain
  // valid until Shutdown is called.
  UserCloudPolicyInvalidator(Profile* profile,
                             CloudPolicyManager* policy_manager);

  // KeyedService:
  void Shutdown() override;

  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // The profile associated with the invalidator.
  Profile* profile_;

  // Used to register for notification that profile creation is complete.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyInvalidator);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_INVALIDATOR_H_
