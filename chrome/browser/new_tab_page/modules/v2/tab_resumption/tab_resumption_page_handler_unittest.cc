// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_page_handler.h"

#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_test_support.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_util.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/mojom/history_types.mojom.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/test/test_web_contents_factory.h"

namespace {

class TabResumptionPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  TabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_session_sync_service_ = static_cast<MockSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<TabResumptionPageHandler>(
        mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>(),
        web_contents_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    mock_session_sync_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  MockSessionSyncService& mock_session_sync_service() {
    return *mock_session_sync_service_;
  }

  TabResumptionPageHandler& handler() { return *handler_; }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SessionSyncServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockSessionSyncService>();
             })}};
  }

  raw_ptr<MockSessionSyncService> mock_session_sync_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabResumptionPageHandler> handler_;
};

TEST_F(TabResumptionPageHandlerTest, GetTabs) {
  const int kSampleSessionsCount = 3;
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  for (int i = 0; i < kSampleSessionsCount; i++) {
    sample_sessions.push_back(SampleSession(
        "Test Name", ("Test Tag " + base::NumberToString(i)).c_str(), 1, 1));
  }

  EXPECT_CALL(*mock_session_sync_service().GetOpenTabsUIDelegate(),
              GetAllForeignSessions(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](
              std::vector<const sync_sessions::SyncedSession*>* sessions) {
            for (auto& sample_session : sample_sessions) {
              sessions->push_back(sample_session.get());
            }
            return true;
          }));

  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&tabs_mojom](std::vector<history::mojom::TabPtr> tabs_arg) {
            tabs_mojom = std::move(tabs_arg);
          }));
  handler().GetTabs(callback.Get());

  ASSERT_EQ(3u, tabs_mojom.size());

  for (unsigned int i = 0; i < kSampleSessionsCount; i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    ASSERT_EQ("Test Tag " + base::NumberToString(i), tab_mojom->session_tag);
    ASSERT_EQ(GURL(kSampleUrl), tab_mojom->url);
  }
}
}  // namespace
