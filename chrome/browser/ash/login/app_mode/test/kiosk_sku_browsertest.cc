// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// workflow: COM_KIOSK_CUJ1_TASK2_WF1
constexpr char kKioskSkuEnrollTag[] =
    "screenplay-0e6fc954-8bad-4da0-ad3c-13579a09653e";

KioskMixin::Config ConfigWithoutAutoLaunchApp() {
  return KioskMixin::Config{
      /*name=*/{},
      /*auto_launch_account_id=*/{},
      // The config needs to have some Kiosk app. Any list of apps would work.
      {KioskMixin::SimpleWebAppOption(), KioskMixin::SimpleChromeAppOption()}};
}

}  // namespace

class KioskSkuEnrollTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskSkuEnrollTest() {
    // Skip initial policy setup. This makes it look like the device is not
    // enrolled yet so we can set the Kiosk SKU license "during" enrollment.
    kiosk_.device_state_mixin().set_skip_initial_policy_setup(true);
  }
  KioskSkuEnrollTest(const KioskSkuEnrollTest&) = delete;
  KioskSkuEnrollTest& operator=(const KioskSkuEnrollTest&) = delete;

  ~KioskSkuEnrollTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    // Update policies with Kiosk apps and the Kiosk SKU license.
    ScopedDevicePolicyUpdate scoped_update(
        policy_helper_.device_policy(), base::BindLambdaForTesting([this]() {
          policy_helper_.RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
        }));
    kiosk_.Configure(scoped_update, ConfigWithoutAutoLaunchApp());
    scoped_update.policy_data()->set_license_sku(policy::kKioskSkuName);
  }

 private:
  ash::KioskMixin kiosk_{&mixin_host_};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

IN_PROC_BROWSER_TEST_F(KioskSkuEnrollTest, IsKioskSkuEnrolled) {
  base::AddFeatureIdTagToTestResult(kKioskSkuEnrollTag);

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  ASSERT_NE(connector, nullptr);
  EXPECT_TRUE(connector->IsCloudManaged());
  EXPECT_TRUE(connector->IsKioskEnrolled());
}

}  // namespace ash
