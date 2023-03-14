// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {
// workflow: COM_KIOSK_CUJ1_TASK2_WF1
constexpr char kKioskSkuEnrollTag[] =
    "screenplay-0e6fc954-8bad-4da0-ad3c-13579a09653e";
}  // namespace

class KioskSkuEnrollTest : public KioskBaseTest {
 public:
  KioskSkuEnrollTest() {
    // skip initial policy setup to change to kiosk sku first
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~KioskSkuEnrollTest() override = default;
  KioskSkuEnrollTest(const KioskSkuEnrollTest&) = delete;
  void operator=(const KioskSkuEnrollTest&) = delete;

  void SetUpOnMainThread() override {
    KioskBaseTest::SetUpOnMainThread();

    // set Kiosk SKU and update policies
    policy_helper_.device_policy()->policy_data().set_license_sku(
        policy::kKioskSkuName);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  }

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

IN_PROC_BROWSER_TEST_F(KioskSkuEnrollTest, IsKioskSkuEnrolled) {
  base::AddFeatureIdTagToTestResult(kKioskSkuEnrollTag);

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  ASSERT_TRUE(connector);
  EXPECT_TRUE(connector->IsCloudManaged());
  EXPECT_TRUE(connector->IsKioskEnrolled());
}

}  // namespace ash
