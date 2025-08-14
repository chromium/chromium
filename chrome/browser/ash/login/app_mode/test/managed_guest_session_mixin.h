// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_MIXIN_H_

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"

namespace ash {

// Helper mixin to enroll a device and set policies to configure a Managed Guest
// Session (MGS) in browser tests.
class ManagedGuestSessionMixin {
 public:
  explicit ManagedGuestSessionMixin(InProcessBrowserTestMixinHost* host);
  ~ManagedGuestSessionMixin();

  ManagedGuestSessionMixin(const ManagedGuestSessionMixin&) = delete;
  ManagedGuestSessionMixin& operator=(const ManagedGuestSessionMixin&) = delete;

  // Configures policies for MGS including what is set in the
  // device_local_account_policy_builder, and waits for device policies to
  // apply.
  void ConfigurePolicies();

  policy::UserPolicyBuilder& device_local_account_policy_builder() {
    CHECK(!policy_applied_)
        << "Set additional policy attributes before calling "
        << "ConfigurePolicies()";
    return device_local_account_policy_;
  }
  AccountId& account_id() { return account_id_; }

 private:
  void AddManagedGuestSessionToDevicePolicy();
  void SetUpDeviceLocalAccountPolicy();

  bool policy_applied_ = false;
  AccountId account_id_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_;
  ash::DeviceStateMixin device_state_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  policy::UserPolicyBuilder device_local_account_policy_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_MANAGED_GUEST_SESSION_MIXIN_H_
