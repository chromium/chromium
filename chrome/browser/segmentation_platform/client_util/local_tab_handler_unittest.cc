// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/client_util/local_tab_handler.h"
#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"
#include "components/segmentation_platform/embedder/tab_fetcher.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/test_browser_window.h"

namespace segmentation_platform::processing {

using ::testing::Return;

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              SubscribeToForeignSessionsChanged,
              (const base::RepeatingClosure& cb),
              (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

class LocalTabHandlerTest : public testing::Test {
 public:
  LocalTabHandlerTest() = default;
  ~LocalTabHandlerTest() override = default;

  void SetUp() override {
    Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    Browser::CreateParams params(profile_.get(), true);
    params.type = Browser::TYPE_NORMAL;
    test_window_ = std::make_unique<TestBrowserWindow>();
    params.window = test_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
    handler_ = std::make_unique<LocalTabHandler>(&session_sync_service_,
                                                 profile_.get());

    source_ = std::make_unique<LocalTabSource>(&session_sync_service_,
                                               handler_.get());

    EXPECT_CALL(session_sync_service_, GetOpenTabsUIDelegate())
        .WillRepeatedly(Return(nullptr));
  }

  void TearDown() override {
    Test::TearDown();
    handler_.reset();
    browser_->tab_strip_model()->CloseAllTabs();
    browser_.reset();
    profile_.reset();
  }

  content::WebContents* AddTab(int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContents* web_contents_ptr = web_contents.get();
    browser_->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    web_contents_ptr->GetController().LoadURL(
        GURL("http://a.com"), content::Referrer(), ui::PAGE_TRANSITION_LINK,
        std::string());
    content::WebContentsTester::For(web_contents_ptr)
        ->CommitPendingNavigation();

    return web_contents_ptr;
  }

  void CloseTab(int index) {
    browser_->tab_strip_model()->CloseWebContentsAt(
        0, TabCloseTypes::CLOSE_USER_GESTURE);
  }

  float GetModifiedTime(const TabFetcher::TabEntry& tab) {
    Tensor inputs(TabSessionSource::kNumInputs, ProcessedValue(0.0f));
    FeatureProcessorState state;
    source_->AddLocalTabInfo(handler_->FindLocalTab(tab), state, inputs);
    return inputs[TabSessionSource::kInputLocalTabTimeSinceModified].float_val;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<BrowserWindow> test_window_;
  std::unique_ptr<LocalTabHandler> handler_;
  std::unique_ptr<LocalTabSource> source_;

  MockSessionSyncService session_sync_service_;
};

TEST_F(LocalTabHandlerTest, EmptyTabs) {
  std::vector<TabFetcher::TabEntry> tabs;
  handler_->FillAllLocalTabsFromTabModel(tabs);
  EXPECT_TRUE(tabs.empty());
}

TEST_F(LocalTabHandlerTest, FetchTabs) {
  auto* webcontents1 = AddTab(0);
  auto* webcontents2 = AddTab(1);

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

TEST_F(LocalTabHandlerTest, LocalTabSource) {
  AddTab(0);
  task_environment_.FastForwardBy(base::Seconds(10));
  AddTab(1);
  task_environment_.FastForwardBy(base::Seconds(10));

  std::vector<TabFetcher::TabEntry> tabs;
  handler_->FillAllLocalTabsFromTabModel(tabs);

  float time1 = GetModifiedTime(tabs[0]);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(/*value=*/20, /*max_buckets=*/50), 16, 0.01);
  EXPECT_NEAR(time1, 16, 0.01);

  float time2 = GetModifiedTime(tabs[1]);
  EXPECT_NEAR(TabSessionSource::BucketizeExp(/*value=*/10, /*max_buckets=*/50), 8, 0.01);
  EXPECT_NEAR(time2, 8, 0.01);
}

}  // namespace segmentation_platform::processing
