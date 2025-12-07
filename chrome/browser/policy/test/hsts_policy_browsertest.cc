// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "url/gurl.h"

namespace policy {

class HSTSPolicyTest : public testing::WithParamInterface<bool>,
                       public PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam()) {
      PolicyMap policies;
      base::Value::List bypass_list;
      bypass_list.Append("example");
      SetPolicy(&policies, key::kHSTSPolicyBypassList,
                base::Value(std::move(bypass_list)));
      provider_.UpdateChromePolicy(policies);
    }
  }

  void TearDownOnMainThread() override {
    if (!content::IsOutOfProcessNetworkService()) {
      base::test::TestFuture<void> future;
      content::GetNetworkTaskRunner()->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&HSTSPolicyTest::CleanUpOnNetworkThread,
                         base::Unretained(this)),
          future.GetCallback());
      EXPECT_TRUE(future.Wait());
    }
  }

  void SetTransportSecurityStateSourceOnNetworkThread() {
    transport_security_state_source_ =
        std::make_unique<net::ScopedTransportSecurityStateSource>();
  }

  void CleanUpOnNetworkThread() { transport_security_state_source_.reset(); }

  // Only used when NetworkService is run in-process.
  // TODO(crbug.com/40649862): NetworkServiceTest doesn't work in
  // browser_tests when using an in-process network service, so the test has
  // to create a ScopedTransportSecurityStateSource directly in that case. If
  // NetworkServiceTest is ever made to work with in-process network service,
  // change the test to use it unconditionally.
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;
};

INSTANTIATE_TEST_SUITE_P(, HSTSPolicyTest, ::testing::Bool());

IN_PROC_BROWSER_TEST_P(HSTSPolicyTest, HSTSPolicyBypassList) {
  if (content::IsOutOfProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::test::TestFuture<void> future;
    network_service_test->SetTransportSecurityStateTestSource(
        true, future.GetCallback());
    EXPECT_TRUE(future.Wait());
  } else {
    base::test::TestFuture<void> future;
    content::GetNetworkTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &HSTSPolicyTest::SetTransportSecurityStateSourceOnNetworkThread,
            base::Unretained(this)),
        future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url("http://example/");
  ASSERT_TRUE(NavigateToUrl(url, this));

  content::WebContents* contents =
      chrome_test_utils::GetActiveWebContents(this);
  if (GetParam()) {
    // If the policy was set, the HSTS upgrade from http://example to
    // https://example should have been disabled, so the url should still be
    // HTTP.
    EXPECT_EQ(url, contents->GetLastCommittedURL());
  } else {
    // If the policy wasn't set, the url should have been upgraded to HTTPS. If
    // this expectation fails, it means that the test wasn't properly using the
    // testing TransportSecurityState data.
    EXPECT_EQ(GURL("https://example/"), contents->GetLastCommittedURL());
  }
}

}  // namespace policy
