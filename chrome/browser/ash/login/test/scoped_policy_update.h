// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"

namespace ash {

// Helper that is used by mixins to provide means for setting up user policy
// values to tests that need that functionality. This does not build, nor apply
// updated policy - that's expected to be done in `callback`.
//
// `callback` - Called when this object goes out of scope.
class ScopedUserPolicyUpdate {
 public:
  explicit ScopedUserPolicyUpdate(policy::UserPolicyBuilder* policy_builder,
                                  base::OnceClosure callback);

  ScopedUserPolicyUpdate(const ScopedUserPolicyUpdate&) = delete;
  ScopedUserPolicyUpdate& operator=(const ScopedUserPolicyUpdate&) = delete;

  ~ScopedUserPolicyUpdate();

  // Policy payload proto - use this to set up desired policy values.
  enterprise_management::CloudPolicySettings* policy_payload() {
    return &policy_builder_->payload();
  }

  // Accessor to the PolicyData message (will contain serialized
  // policy_payload() among other things).
  enterprise_management::PolicyData* policy_data() {
    return &policy_builder_->policy_data();
  }

 private:
  const raw_ptr<policy::UserPolicyBuilder> policy_builder_;
  base::OnceClosure callback_;
};

// Helper that is used by mixins to provide means for setting up device policy
// values to tests that need that functionality. This does not build, nor apply
// updated policy - that's expected to be done in `callback`.
//
// `callback` - Called when this object goes out of scope.
class ScopedDevicePolicyUpdate {
 public:
  explicit ScopedDevicePolicyUpdate(policy::DevicePolicyBuilder* policy_builder,
                                    base::OnceClosure callback);

  ScopedDevicePolicyUpdate(const ScopedDevicePolicyUpdate&) = delete;
  ScopedDevicePolicyUpdate& operator=(const ScopedDevicePolicyUpdate&) = delete;

  ~ScopedDevicePolicyUpdate();

  // Policy payload proto - use this to set up desired policy values.
  enterprise_management::ChromeDeviceSettingsProto* policy_payload() {
    return &policy_builder_->payload();
  }

  // The policy data that will be used to build the policy.
  enterprise_management::PolicyData* policy_data() {
    return &policy_builder_->policy_data();
  }

 private:
  const raw_ptr<policy::DevicePolicyBuilder> policy_builder_;
  base::OnceClosure callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_
