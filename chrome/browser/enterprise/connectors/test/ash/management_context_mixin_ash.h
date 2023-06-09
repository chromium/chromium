// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ASH_MANAGEMENT_CONTEXT_MIXIN_ASH_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ASH_MANAGEMENT_CONTEXT_MIXIN_ASH_H_

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/enterprise/connectors/test/management_context_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace enterprise_connectors::test {

class ManagementContextMixinAsh : public ManagementContextMixin {
 public:
  ManagementContextMixinAsh(InProcessBrowserTestMixinHost* host,
                            InProcessBrowserTest* test_base,
                            ManagementContext management_context);

  ManagementContextMixinAsh(const ManagementContextMixinAsh&) = delete;
  ManagementContextMixinAsh& operator=(const ManagementContextMixinAsh&) =
      delete;

  ~ManagementContextMixinAsh() override;

  // ManagementContextMixin:
  std::unique_ptr<ash::ScopedDevicePolicyUpdate> RequestDevicePolicyUpdate()
      override;
  void ManageCloudUser() override;

 protected:
  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  // ManagementContextMixin:
  void ManageCloudMachine() override;

 private:
  ash::DeviceStateMixin device_state_mixin_;
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_ASH_MANAGEMENT_CONTEXT_MIXIN_ASH_H_
