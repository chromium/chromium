// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/accessibility/browser_test_with_ax_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

namespace {

// A test fixture that runs an accessibility client in an external process. This
// is used to perform various interactions with the browser to ensure that it is
// well-behaved.
// Use --vmodule=leaked_node_detection_browsertest=1,ax_client*=1 when running
// tests based on this fixture to get log output regarding what's going on.
class LeakedNodeDetectionBrowsertest
    : public BrowserTestWithAxClient,
      public testing::WithParamInterface<BrowserTestWithAxClient::ClientApi> {
 protected:
  static BrowserTestWithAxClient::ClientApi client_api() { return GetParam(); }

  LeakedNodeDetectionBrowsertest();

 private:
  ClientApi GetClientApi() const override { return client_api(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

LeakedNodeDetectionBrowsertest::LeakedNodeDetectionBrowsertest() {
  scoped_feature_list_.InitWithFeatures(
      {features::kUiaProvider, features::kUiaDisconnectRootProviders}, {});
}

}  // namespace

IN_PROC_BROWSER_TEST_P(LeakedNodeDetectionBrowsertest, DetectGhostNodeLeaks) {
  // Initialize the UI Automation client; giving it this window.
  ASSERT_HRESULT_SUCCEEDED(InitializeClient(browser()));

  // Tell the client to slurp up everything in the window.
  ASSERT_HRESULT_SUCCEEDED(FindAll());

  // Let some time pass so that any straggler events from the browser can reach
  // the AxClient's event handler(s) and bounce around in it. All of this may
  // result in COM messages being processed on the browser's UI thread.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  VLOG(1) << NodeCountsToString(ui::AXPlatformNodeWin::GetCounts());

  // Close the browser window while objects are held.
  CloseBrowserSynchronously(browser());

  // Wait for all nodes to be destroyed.
  WaitForNodeCounts({0U, 0U, 0U, 0U});
}

// TODO(crbug.com/443107137): Disabled due to flaky failures. It seems that not
// all machines perform rundown quckily upon disappearance of the UiaClient
// despite all classes implementing IFastRundown. Run this test with
// --test-launcher-timeout=3600000 --ui-test-action-max-timeout=3600000
// --ui-test-action-timeout=3600000 --single-process-tests to set the various
// test timeouts longer than the approx six minute slow rundown time.
IN_PROC_BROWSER_TEST_P(LeakedNodeDetectionBrowsertest,
                       DISABLED_TerminateClient) {
  // Initialize the UI Automation client; giving it this window.
  ASSERT_HRESULT_SUCCEEDED(InitializeClient(browser()));

  // Tell the client to slurp up everything in the window.
  ASSERT_HRESULT_SUCCEEDED(FindAll());

  // Let some time pass so that any straggler events from the browser can reach
  // the AxClient's event handler(s) and bounce around in it. All of this may
  // result in COM messages being processed on the browser's UI thread.
  {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  VLOG(1) << NodeCountsToString(ui::AXPlatformNodeWin::GetCounts());

  // Terminate the client without letting it gracefully clean up.
  TerminateClient();

  // Close the browser window.
  CloseBrowserSynchronously(browser());

  // Wait for all nodes to be destroyed.
  WaitForNodeCounts({0U, 0U, 0U, 0U});
}

INSTANTIATE_TEST_SUITE_P(
    UiaClient,
    LeakedNodeDetectionBrowsertest,
    testing::Values(BrowserTestWithAxClient::ClientApi::kUiAutomation));

INSTANTIATE_TEST_SUITE_P(
    Ia2Client,
    LeakedNodeDetectionBrowsertest,
    testing::Values(BrowserTestWithAxClient::ClientApi::kIAccessible2));
