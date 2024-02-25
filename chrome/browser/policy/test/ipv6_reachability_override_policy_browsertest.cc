// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "url/gurl.h"

namespace policy {

class IPv6ReachabilityOverridePolicyTest : public PolicyTest {
 public:
  IPv6ReachabilityOverridePolicyTest() = default;
  ~IPv6ReachabilityOverridePolicyTest() override = default;

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    // Clear host resolver's rules as PolicyTest::SetUpOnMainThread() adds a
    // wildcard rule to resolve 127.0.0.1.
    host_resolver()->ClearRules();
    host_resolver()->AddRule("ipv6.test", "::1");

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start(/*port=*/0, /*address=*/"::1"));
  }

 protected:
  void SimulateIPv6ProbeFailure() {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());

    base::RunLoop loop;
    network_service_test->SetIPv6ProbeResult(false, loop.QuitClosure());
    loop.Run();
  }

  int LoadIPv6OnlyRequest() {
    GURL url(base::StringPrintf("http://ipv6.test:%hu/empty.html",
                                embedded_test_server()->port()));
    return content::LoadBasicRequest(
        browser()->profile()->GetDefaultStoragePartition()->GetNetworkContext(),
        url);
  }
};

IN_PROC_BROWSER_TEST_F(IPv6ReachabilityOverridePolicyTest, EnableOverride) {
  SimulateIPv6ProbeFailure();

  int rv = LoadIPv6OnlyRequest();
  EXPECT_EQ(rv, net::ERR_NAME_NOT_RESOLVED);

  PolicyMap policies;
  policies.Set(key::kIPv6ReachabilityOverrideEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(true),
               /*external_data_fetcher=*/nullptr);
  UpdateProviderPolicy(policies);

  rv = LoadIPv6OnlyRequest();
  EXPECT_EQ(rv, net::OK);
}

}  // namespace policy
