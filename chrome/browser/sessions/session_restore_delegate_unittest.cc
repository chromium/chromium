// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_delegate.h"

#include "chrome/browser/performance_manager/public/background_tab_loading_policy.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/performance_manager/test_support/page_node_utils.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::content::BrowserContext;
using ::content::NavigationEntry;
using ::content::WebContents;
using ::content::WebContentsTester;
using ::favicon::ContentFaviconDriver;
using ::favicon::MockFaviconService;
using ::performance_manager::PerformanceManagerTestHarnessHelper;
using ::testing::Bool;
using ::testing::NiceMock;
using ::testing::WithParamInterface;

std::unique_ptr<WebContents> CreateRestoredWebContents(
    BrowserContext* browser_context) {
  std::unique_ptr<WebContents> test_contents =
      WebContentsTester::CreateTestWebContents(browser_context, nullptr);
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(NavigationEntry::Create());
  test_contents->GetController().Restore(0, content::RestoreType::kRestored,
                                         &entries);

  performance_manager::testing::SetPageNodeType(
      performance_manager::testing::GetPageNodeForWebContents(
          test_contents.get()),
      performance_manager::PageType::kTab);
  return test_contents;
}

class RestoreTabsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    perf_manager_helper_.SetUp();
  }
  void TearDown() override {
    perf_manager_helper_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  PerformanceManagerTestHarnessHelper perf_manager_helper_;
};

TEST_F(RestoreTabsTest, RestoreTabsRestoresFavicons) {
  performance_manager::policies::InstallBackgroundTabLoadingPolicyForTesting(
      base::BindRepeating(&SessionRestore::OnTabLoaderFinishedLoadingTabs));

  SetContents(CreateRestoredWebContents(browser_context()));

  // Setup a mock favicon service.
  NiceMock<MockFaviconService> favicon_service;
  ContentFaviconDriver::CreateForWebContents(web_contents(), &favicon_service);

  // Restoring a tab should fetch a favicon.
  EXPECT_CALL(favicon_service, GetFaviconForPageURL);
  SessionRestoreDelegate::RestoreTabs(
      {SessionRestoreDelegate::RestoredTab(
          web_contents(), /*is_active=*/false, /*is_app=*/false,
          /*is_pinned=*/false, /*group=*/std::nullopt, /*split=*/std::nullopt)},
      base::TimeTicks::Now());
}

}  // namespace
