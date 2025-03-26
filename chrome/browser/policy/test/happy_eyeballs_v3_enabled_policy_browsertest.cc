// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "url/gurl.h"

namespace policy {

class HappyEyeballsV3EnabledPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<
          /*policy::key::kHappyEyeballsV3Enabled=*/std::optional<bool>> {
 public:
  HappyEyeballsV3EnabledPolicyTest() = default;

  ~HappyEyeballsV3EnabledPolicyTest() override = default;

  void SetUp() override {
    if (GetParam().has_value()) {
      PolicyMap policies;
      policies.Set(policy::key::kHappyEyeballsV3Enabled, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                   base::Value(*GetParam()),
                   /*external_data_fetcher=*/nullptr);
      UpdateProviderPolicy(policies);
    }

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    PolicyTest::SetUp();
  }

 protected:
  bool IsHappyEyeballsV3EnabledInNetworkService() {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());

    base::RunLoop loop;
    std::optional<bool> enabled;
    network_service_test->IsHappyEyeballsV3Enabled(
        base::BindLambdaForTesting([&](bool result) {
          enabled = result;
          loop.Quit();
        }));
    loop.Run();

    return *enabled;
  }

  void TestDynamicRefresh(bool enabled) {
    if (GetParam().value_or(
            base::FeatureList::IsEnabled(net::features::kHappyEyeballsV3))) {
      ASSERT_TRUE(IsHappyEyeballsV3EnabledInNetworkService());
    } else {
      ASSERT_FALSE(IsHappyEyeballsV3EnabledInNetworkService());
    }

    PolicyMap policies;
    policies.Set(policy::key::kHappyEyeballsV3Enabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enabled),
                 /*external_data_fetcher=*/nullptr);
    UpdateProviderPolicy(policies);
    ASSERT_EQ(IsHappyEyeballsV3EnabledInNetworkService(), enabled);

    // Ensure a loading request succeeds.
    GURL url = embedded_test_server()->GetURL("/empty.html");
    int rv = content::LoadBasicRequest(
        browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext(),
        url);
    ASSERT_EQ(rv, net::OK);
  }
};

IN_PROC_BROWSER_TEST_P(HappyEyeballsV3EnabledPolicyTest, RespectPolicy) {
  if (GetParam().value_or(
          base::FeatureList::IsEnabled(net::features::kHappyEyeballsV3))) {
    ASSERT_TRUE(IsHappyEyeballsV3EnabledInNetworkService());
  } else {
    ASSERT_FALSE(IsHappyEyeballsV3EnabledInNetworkService());
  }

  // Ensure a loading request succeeds.
  GURL url = embedded_test_server()->GetURL("/empty.html");
  int rv = content::LoadBasicRequest(
      browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext(),
      url);
  ASSERT_EQ(rv, net::OK);
}

IN_PROC_BROWSER_TEST_P(HappyEyeballsV3EnabledPolicyTest, DynamicRefreshEnable) {
  TestDynamicRefresh(true);
}

IN_PROC_BROWSER_TEST_P(HappyEyeballsV3EnabledPolicyTest,
                       DynamicRefreshDisable) {
  TestDynamicRefresh(false);
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    HappyEyeballsV3EnabledPolicyTest,
    ::testing::Values(/*policy::key::kHappyEyeballsV3Enabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    HappyEyeballsV3EnabledPolicyTest,
    ::testing::Values(/*policy::key::kHappyEyeballsV3Enabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    HappyEyeballsV3EnabledPolicyTest,
    ::testing::Values(
        /*policy::key::kHappyEyeballsV3Enabled=*/std::nullopt));

}  // namespace policy
