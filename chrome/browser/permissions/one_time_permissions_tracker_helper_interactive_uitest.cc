// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"

class OneTimePermissionsTrackerHelperBrowserTest
    : public InteractiveBrowserTest {
 public:
  // InteractiveBrowserTest:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Regression test for crbug.com/396324286.
IN_PROC_BROWSER_TEST_F(OneTimePermissionsTrackerHelperBrowserTest,
                       TabDiscardDuringPendingNavigationEmitsUnloadCorrectly) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  const GURL url_1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url_2 = embedded_test_server()->GetURL("/title2.html");
  const GURL url_3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      // Instrument 2 tabs, select the first.
      InstrumentTab(kFirstTab), AddInstrumentedTab(kSecondTab, url_1),
      SelectTab(kTabStripElementId, 0),

      // Ensure both contents have finished navigation.
      NavigateWebContents(kFirstTab, url_1), WaitForWebContentsReady(kFirstTab),
      WaitForWebContentsReady(kSecondTab),

      // Navigate the selected tab to another URL, do not wait for the
      // navigation to finish.
      Do(base::BindLambdaForTesting([&]() {
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), url_3, WindowOpenDisposition::CURRENT_TAB,
            ui_test_utils::BROWSER_TEST_NO_WAIT);
      })),

      // Immediately switch to the second tab and discard the first.
      SelectTab(kTabStripElementId, 1), Do(base::BindLambdaForTesting([&]() {
        auto* lifecycle_unit = resource_coordinator::TabLifecycleUnitSource::
            GetTabLifecycleUnitExternal(
                browser()->tab_strip_model()->GetWebContentsAt(0));
        lifecycle_unit->DiscardTab(mojom::LifecycleUnitDiscardReason::EXTERNAL);
      })));

  // There should not be a crash.
}
