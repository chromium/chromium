// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/local_tab_handler.h"

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/sync_sessions/mock_session_sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace segmentation_platform::processing {

using ::testing::Return;
using MockSessionSyncService = sync_sessions::MockSessionSyncService;

class LocalTabHandlerTest : public InProcessBrowserTest {
 public:
  LocalTabHandlerTest() = default;
  ~LocalTabHandlerTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    handler_ = std::make_unique<LocalTabHandler>(&session_sync_service_,
                                                 browser()->profile());

    source_ = std::make_unique<LocalTabSource>(&session_sync_service_,
                                               handler_.get());

    EXPECT_CALL(session_sync_service_, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(nullptr));
  }

  void TearDownOnMainThread() override {
    handler_.reset();
    source_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* AddTab() {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("about:blank"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    return browser()->tab_strip_model()->GetWebContentsAt(
        browser()->tab_strip_model()->count() - 1);
  }

  void CloseTab(int index) {
    browser()->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_USER_GESTURE);
  }

  float GetModifiedTime(const TabFetcher::TabEntry& tab) {
    Tensor inputs(TabSessionSource::kNumInputs, ProcessedValue(0.0f));
    FeatureProcessorState state;
    source_->AddLocalTabInfo(handler_->FindLocalTab(tab), state, inputs);
    return inputs[TabSessionSource::kInputLocalTabTimeSinceModified].float_val;
  }

 protected:
  std::unique_ptr<LocalTabHandler> handler_;
  std::unique_ptr<LocalTabSource> source_;

  MockSessionSyncService session_sync_service_;
};

IN_PROC_BROWSER_TEST_F(LocalTabHandlerTest, EmptyTabs) {
  std::vector<TabFetcher::TabEntry> tabs;
  handler_->FillAllLocalTabsFromTabModel(tabs);
  // We expect 1 tab (the default one created by the browser test fixture).
  EXPECT_EQ(tabs.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(LocalTabHandlerTest, FetchTabs) {
  auto* webcontents1 = AddTab();
  auto* webcontents2 = AddTab();

  // Close the default tab (at index 0) to match the expected size of 2.
  CloseTab(0);

  std::vector<TabFetcher::TabEntry> tabs1;
  handler_->FillAllLocalTabsFromTabModel(tabs1);
  ASSERT_EQ(tabs1.size(), 2u);
  EXPECT_EQ(tabs1[0].web_contents_data, webcontents1);
  EXPECT_EQ(tabs1[1].web_contents_data, webcontents2);

  auto find_tab1 = handler_->FindLocalTab(tabs1[0]);
  EXPECT_EQ(find_tab1.session_tab, nullptr);
  EXPECT_EQ(find_tab1.webcontents, webcontents1);

  CloseTab(0);

  std::vector<TabFetcher::TabEntry> tabs2;
  handler_->FillAllLocalTabsFromTabModel(tabs2);
  ASSERT_EQ(tabs2.size(), 1u);
  EXPECT_EQ(tabs2[0].web_contents_data, webcontents2);

  auto find_tab2 = handler_->FindLocalTab(tabs1[0]);
  EXPECT_EQ(find_tab2.session_tab, nullptr);
  EXPECT_EQ(find_tab2.webcontents, nullptr);

  auto find_tab3 = handler_->FindLocalTab(tabs1[1]);
  EXPECT_EQ(find_tab3.session_tab, nullptr);
  EXPECT_EQ(find_tab3.webcontents, webcontents2);
}

IN_PROC_BROWSER_TEST_F(LocalTabHandlerTest, LocalTabSource) {
  auto* web_contents1 = AddTab();
  auto* entry1 = web_contents1->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry1);
  entry1->SetTimestamp(base::Time::Now() - base::Seconds(20));

  auto* web_contents2 = AddTab();
  auto* entry2 = web_contents2->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry2);
  entry2->SetTimestamp(base::Time::Now() - base::Seconds(10));

  // Close the default tab (at index 0) to match the expected size of 2.
  CloseTab(0);

  std::vector<TabFetcher::TabEntry> tabs;
  handler_->FillAllLocalTabsFromTabModel(tabs);

  ASSERT_EQ(tabs.size(), 2u);

  float time1 = GetModifiedTime(tabs[0]);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(/*value=*/20, /*max_buckets=*/50),
              16, 0.1);
  EXPECT_NEAR(time1, 16, 0.1);

  float time2 = GetModifiedTime(tabs[1]);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(/*value=*/10, /*max_buckets=*/50),
              8, 0.1);
  EXPECT_NEAR(time2, 8, 0.1);
}

}  // namespace segmentation_platform::processing
