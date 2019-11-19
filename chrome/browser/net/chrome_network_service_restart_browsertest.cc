// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/cpp/features.h"

namespace content {

// |ChromeNetworkServiceRestartBrowserTest| is required to test Chrome specific
// code such as |ChromeContentBrowserClient|.
// See |NetworkServiceRestartBrowserTest| for content's version of tests.
class ChromeNetworkServiceRestartBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNetworkServiceRestartBrowserTest() {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestURL() const {
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    return embedded_test_server()->GetURL("/echoheader");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeNetworkServiceRestartBrowserTest);
};

// Make sure |StoragePartition::GetNetworkContext()| returns valid interface
// after crash.
IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceRestartBrowserTest,
                       StoragePartitionGetNetworkContext) {
  if (content::IsInProcessNetworkService())
    return;
#if defined(OS_MACOSX)
  // |NetworkServiceTestHelper| doesn't work on browser_tests on macOS.
  return;
#endif
  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser()->profile());

  network::mojom::NetworkContext* old_network_context =
      partition->GetNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(old_network_context, GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // |partition->GetNetworkContext()| should return a valid new pointer after
  // crash.
  EXPECT_NE(old_network_context, partition->GetNetworkContext());
  EXPECT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), GetTestURL()));
}

// Make sure |SystemNetworkContextManager::GetContext()| returns valid interface
// after crash.
IN_PROC_BROWSER_TEST_F(ChromeNetworkServiceRestartBrowserTest,
                       SystemNetworkContextManagerGetContext) {
  if (content::IsInProcessNetworkService())
    return;
#if defined(OS_MACOSX)
  // |NetworkServiceTestHelper| doesn't work on browser_tests on macOS.
  return;
#endif
  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();

  EXPECT_EQ(net::OK,
            LoadBasicRequest(system_network_context_manager->GetContext(),
                             GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  system_network_context_manager->FlushNetworkInterfaceForTesting();

  // |system_network_context_manager->GetContext()| should return a valid
  // pointer after crash, since the NetworkContext is bound again.
  ASSERT_NE(system_network_context_manager->GetContext(), nullptr);
  EXPECT_EQ(net::OK,
            LoadBasicRequest(system_network_context_manager->GetContext(),
                             GetTestURL()));
}

}  // namespace content
