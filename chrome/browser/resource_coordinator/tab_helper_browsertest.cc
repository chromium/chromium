// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include "base/functional/bind.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {
namespace {

class MockLoadingStateHelper : public TabLoadTracker::Observer {
 public:
  MockLoadingStateHelper() {
    resource_coordinator::TabLoadTracker::Get()->AddObserver(this);
  }
  ~MockLoadingStateHelper() override {
    resource_coordinator::TabLoadTracker::Get()->RemoveObserver(this);
  }

  MOCK_METHOD(void,
              OnLoadingStateChange,
              (content::WebContents * web_contents,
               LoadingState old_loading_state,
               LoadingState new_loading_state),
              (override));
};

}  // namespace

class TabHelperBrowserTest : public InProcessBrowserTest {
 public:
  TabHelperBrowserTest()
      : prerender_test_helper_(
            base::BindRepeating(&TabHelperBrowserTest::GetWebContents,
                                base::Unretained(this))) {}
  ~TabHelperBrowserTest() override = default;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    test_server_handle_ = embedded_test_server()->StartAndReturnHandle();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForTabsToLoad() {
    resource_coordinator::WaitForTransitionToLoaded(GetWebContents());
  }

 private:
  content::test::PrerenderTestHelper prerender_test_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

// Tests that `ukm_source_id_` from ResourceCoordinatorTabHelper is updated and
// used only with the active page loading. Since `ukm_source_id_` is used in
// TabManagerStatsCollector::OnTabIsLoaded() triggered by
// OnLoadingStateChange(), this test checks that OnLoadingStateChange() is not
// called in prerendering.
IN_PROC_BROWSER_TEST_F(TabHelperBrowserTest,
                       TabHelperBrowserTestInPrerendering) {
  // Ensure that state change for the previous loading is completed.
  WaitForTabsToLoad();

  // Initial navigation.
  MockLoadingStateHelper mock_loading_state_helper;
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  // The state is changed twice from LOADED to LOADING and from LOADING to
  // LOADED.
  EXPECT_CALL(mock_loading_state_helper, OnLoadingStateChange).Times(2);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  resource_coordinator::ResourceCoordinatorTabHelper* observer =
      resource_coordinator::ResourceCoordinatorTabHelper::FromWebContents(
          GetWebContents());
  WaitForTabsToLoad();
  ukm::SourceId initial_source_id = observer->ukm_source_id();
  // UKM source id is updated by the initial loading.
  DCHECK_NE(initial_source_id, ukm::kInvalidSourceId);

  // Load a page in the prerender.
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  // The prerendering doesn't trigger any loading state change.
  EXPECT_CALL(mock_loading_state_helper, OnLoadingStateChange).Times(0);
  prerender_helper().AddPrerender(prerender_url);
  // The source id should not be updated in prerendering.
  DCHECK_EQ(initial_source_id, observer->ukm_source_id());

  // The state is changed twice from LOADED to LOADING and from LOADING to
  // LOADED.
  EXPECT_CALL(mock_loading_state_helper, OnLoadingStateChange).Times(2);
  // Activate the prerendered page.
  prerender_helper().NavigatePrimaryPage(prerender_url);
  WaitForTabsToLoad();
  // The source id should be updated by changing primary page.
  DCHECK_NE(initial_source_id, observer->ukm_source_id());
}

}  // namespace resource_coordinator
