// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace policy {

// Similar to PolicyTest but sets a couple of policies before the browser is
// started.
class PlatformPolicyManagementServiceTest : public PolicyTest {
 public:
  PlatformPolicyManagementServiceTest() = default;
  ~PlatformPolicyManagementServiceTest() override = default;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Some testing machines/bots are managed, so we cannot test that they become
// managed after setting one policy. For those machines we check the presence of
// `EnterpriseManagementAuthority::COMPUTER_LOCAL` in the management
// authorities. For other machines we test that the management authority
// trustworthiness becomes `ManagementAuthorityTrustworthiness::LOW`.
IN_PROC_BROWSER_TEST_F(PlatformPolicyManagementServiceTest, HasPolicy) {
  bool testing_on_managed_device =
      ManagementServiceFactory::GetForPlatform()->IsManaged();
  PolicyMap policies;
  policies.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(ManagementServiceFactory::GetForPlatform()->IsManaged());
  EXPECT_TRUE(
      ManagementServiceFactory::GetForPlatform()->HasManagementAuthority(
          EnterpriseManagementAuthority::COMPUTER_LOCAL));
  if (!testing_on_managed_device) {
    EXPECT_EQ(ManagementAuthorityTrustworthiness::LOW,
              ManagementServiceFactory::GetForPlatform()
                  ->GetManagementAuthorityTrustworthiness());
  }
}
#endif

}  //  namespace policy
