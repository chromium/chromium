// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "url/gurl.h"

namespace policy {

class HSTSPolicyTest : public PolicyTest {
  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    std::vector<base::Value> bypass_list;
    bypass_list.emplace_back(base::Value("example"));
    SetPolicy(&policies, key::kHSTSPolicyBypassList,
              base::ListValue(bypass_list));
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(HSTSPolicyTest, HSTSPolicyBypassList) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  // The port number 1234 here doesn't matter - it just needs to be a non-zero
  // value so that we use the unittest_default preload list.
  network_service_test->SetTransportSecurityStateSource(1234);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url("http://example/");
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // If the policy didn't take effect, the request to http://example would be
  // upgraded to https://example. This checks that the HSTS upgrade to https
  // didn't happen.
  EXPECT_EQ(url, contents->GetURL());
}

}  // namespace policy
