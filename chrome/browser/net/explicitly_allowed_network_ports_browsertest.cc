// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/base/port_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"

namespace {

using policy::PolicyMap;

class ExplicitlyAllowedNetworkPortsBrowserTest : public policy::PolicyTest {
 protected:
  ExplicitlyAllowedNetworkPortsBrowserTest() : scoped_allowable_port_(79) {}

  network::mojom::NetworkContext* network_context() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  void EnablePort79ByPolicy() {
    PolicyMap policies;
    base::Value::List list;
    // Port 25 is just ignored, because it is not on the allowable ports list.
    list.Append("25");
    list.Append("79");
    SetPolicy(&policies, policy::key::kExplicitlyAllowedNetworkPorts,
              base::Value(std::move(list)));
    UpdateProviderPolicy(policies);
  }

 private:
  net::ScopedAllowablePortForTesting scoped_allowable_port_;
};

// The fact that port 79 is blocked by default is verified in the browsertest
// CommandLineFlagsBrowserTest.Port79DefaultBlocked, so we don't retest it here.

// Tests that the policy is successfully sent to the network service.
//
// The request may succeed or fail depending on the platform and what services
// are running, so the test just verifies the reason for failure is not
// ERR_UNSAFE_PORT.
IN_PROC_BROWSER_TEST_F(ExplicitlyAllowedNetworkPortsBrowserTest,
                       Unblock79Succeeds) {
  EnablePort79ByPolicy();

  // This might still be racy because the network process might not apply the
  // policy before we make the network request. It's unclear how to fix this if
  // it happens.

  EXPECT_NE(net::ERR_UNSAFE_PORT,
            content::LoadBasicRequest(network_context(),
                                      GURL("http://127.0.0.1:79")));
}

class ExplicitlyAllowedNetworkPortsBackgroundFetchBrowserTest
    : public ExplicitlyAllowedNetworkPortsBrowserTest {
 public:
  std::string PerformBackgroundFetch() {
    auto handle = embedded_test_server()->StartAndReturnHandle();
    // We don't actually use the functions on this page, we just need a URL
    // that is in the right scope for the service worker.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/background_fetch/background_fetch.html")));

    return EvalJs(chrome_test_utils::GetActiveWebContents(this), R"(
(async () => {
  await navigator.serviceWorker.register('/background_fetch/sw.js');
  const registration = await navigator.serviceWorker.ready;
  try {
    await registration.backgroundFetch.fetch(
      'bg-fetch-id', 'http://localhost:79/background_fetch/types_of_cheese.txt');
    return 'NOT BLOCKED';
  } catch (e) {
    if (e.name === 'TypeError' &&
        e.message.endsWith('that port is not allowed.')) {
      return 'BLOCKED';
    } else {
      throw(e);
    }
  }
})();
)")
        .ExtractString();
  }
};

// Tests that the policy is successfully sent to the render process. There
// aren't actually many APIs in the render process that use the restricted port
// list. BackgroundFetch is probably the most convenient, although it requires a
// service worker. If BackgroundFetch stops using the restricted port list then
// this test will stop working and we will have to find another API to use.

// First verify that BackgroundFetch still throws an exception for blocked
// ports.
IN_PROC_BROWSER_TEST_F(ExplicitlyAllowedNetworkPortsBackgroundFetchBrowserTest,
                       BlockedPortsThrow) {
  EXPECT_EQ(PerformBackgroundFetch(), "BLOCKED");
}

IN_PROC_BROWSER_TEST_F(ExplicitlyAllowedNetworkPortsBackgroundFetchBrowserTest,
                       UnblockedPortsDontThrow) {
  EnablePort79ByPolicy();

  // This might still be racy because the render process might not apply the
  // policy before we run this JavaScript. It's unclear how to fix this if it
  // happens.

  EXPECT_EQ(PerformBackgroundFetch(), "NOT BLOCKED");
}

}  // namespace
