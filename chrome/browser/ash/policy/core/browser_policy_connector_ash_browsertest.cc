// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;

namespace policy {

const char kCustomDisplayDomain[] = "acme.corp";
const char kMachineName[] = "machine_name";
const char kCustomManager[] = "user@acme.corp";

class BrowserPolicyConnectorAshTest : public DevicePolicyCrosBrowserTest {
 public:
  BrowserPolicyConnectorAshTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~BrowserPolicyConnectorAshTest() override = default;
};

// Test that GetEnterpriseEnrollmentDomain is returned for
// GetEnterpriseEnrollmentDomain and for GetEnterpriseDomainManager if the
// policy doesn't have any domain/manager information.
IN_PROC_BROWSER_TEST_F(BrowserPolicyConnectorAshTest, EnrollmentDomain) {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  EXPECT_EQ(PolicyBuilder::kFakeDomain,
            connector->GetEnterpriseEnrollmentDomain());

  // If no manager or display domain set, EnterpriseDomainManager is equal to
  // EnterpriseEnrollmentDomain
  EXPECT_EQ(connector->GetEnterpriseEnrollmentDomain(),
            connector->GetEnterpriseDomainManager());
}

// Test that GetEnterpriseDomainManager returns the policy display_domain if
// no managed_by value is set.
IN_PROC_BROWSER_TEST_F(BrowserPolicyConnectorAshTest, DisplayDomain) {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  device_policy()->policy_data().set_display_domain(kCustomDisplayDomain);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  // At this point display domain is set and policy is loaded so expect to see
  /// the display domain.
  EXPECT_EQ(kCustomDisplayDomain, connector->GetEnterpriseDomainManager());

  // Make sure that enrollment domain stays the same.
  EXPECT_EQ(PolicyBuilder::kFakeDomain,
            connector->GetEnterpriseEnrollmentDomain());
}

// Test that GetEnterpriseDomainManager returns the policy managed_by if
// it is set.
IN_PROC_BROWSER_TEST_F(BrowserPolicyConnectorAshTest, ManagedBy) {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  device_policy()->policy_data().set_display_domain(kCustomDisplayDomain);
  device_policy()->policy_data().set_managed_by(kCustomManager);

  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  // Now that the managed_by is set expect to see that.
  EXPECT_EQ(kCustomManager, connector->GetEnterpriseDomainManager());

  // Make sure that enrollment domain stays the same.
  EXPECT_EQ(PolicyBuilder::kFakeDomain,
            connector->GetEnterpriseEnrollmentDomain());
}

IN_PROC_BROWSER_TEST_F(BrowserPolicyConnectorAshTest, MarketSegment) {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  EXPECT_EQ(MarketSegment::UNKNOWN, connector->GetEnterpriseMarketSegment());

  device_policy()->policy_data().set_market_segment(
      enterprise_management::PolicyData::ENROLLED_EDUCATION);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  EXPECT_EQ(MarketSegment::EDUCATION, connector->GetEnterpriseMarketSegment());
}

IN_PROC_BROWSER_TEST_F(BrowserPolicyConnectorAshTest, MachineName) {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  EXPECT_EQ(std::string(), connector->GetMachineName());
  device_policy()->policy_data().set_machine_name(kMachineName);
  policy_helper()->RefreshPolicyAndWaitUntilDeviceCloudPolicyUpdated();
  // At this point custom display domain is set and policy is loaded so expect
  // to see the custom display domain.
  EXPECT_EQ(kMachineName, connector->GetMachineName());
}

}  // namespace policy
