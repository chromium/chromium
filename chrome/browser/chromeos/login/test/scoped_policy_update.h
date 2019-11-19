// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "components/policy/core/common/cloud/policy_builder.h"

namespace chromeos {

// Helper that is used by mixins to provide means for setting up user policy
// values to tests that need that functionality. This does not build, nor apply
// updated policy - that's expected to be done in |callback|.
//
// |callback| - Called when this object goes out of scope.
class ScopedUserPolicyUpdate {
 public:
  explicit ScopedUserPolicyUpdate(policy::UserPolicyBuilder* policy_builder,
                                  base::OnceClosure callback);
  ~ScopedUserPolicyUpdate();

  // Policy payload proto - use this to set up desired policy values.
  enterprise_management::CloudPolicySettings* policy_payload() {
    return &policy_builder_->payload();
  }

 private:
  policy::UserPolicyBuilder* const policy_builder_;
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUserPolicyUpdate);
};

// Helper that is used by mixins to provide means for setting up device policy
// values to tests that need that functionality. This does not build, nor apply
// updated policy - that's expected to be done in |callback|.
//
// |callback| - Called when this object goes out of scope.
class ScopedDevicePolicyUpdate {
 public:
  explicit ScopedDevicePolicyUpdate(policy::DevicePolicyBuilder* policy_builder,
                                    base::OnceClosure callback);
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
  policy::DevicePolicyBuilder* const policy_builder_;
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDevicePolicyUpdate);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_SCOPED_POLICY_UPDATE_H_
