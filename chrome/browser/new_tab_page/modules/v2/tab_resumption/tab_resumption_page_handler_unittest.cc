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

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<TabResumptionPageHandler>(
        mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>(),
        profile(), web_contents_.get());
    mock_session_sync_service_ = static_cast<MockSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  MockSessionSyncService& mock_session_sync_service() {
    return *mock_session_sync_service_;
  }

  TabResumptionPageHandler& handler() { return *handler_; }

  void ResetHandler() { handler_.reset(); }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{SessionSyncServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockSessionSyncService>();
             })}};
  }

  raw_ptr<MockSessionSyncService, DisableDanglingPtrDetection>
      mock_session_sync_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabResumptionPageHandler> handler_;
};

// TODO(mfacey): Figure out why test breaks on exit.
TEST_F(TabResumptionPageHandlerTest, DISABLED_GetTabs) {
  const int kSampleSessionsCount = 3;
  std::vector<const sync_sessions::SyncedSession*> sample_sessions;
  for (int i = 0; i < kSampleSessionsCount; i++) {
    sample_sessions.push_back(
        SampleSession(("Test Tag " + base::NumberToString(i)).c_str(), 3));
  }

  EXPECT_CALL(*mock_session_sync_service().GetOpenTabsUIDelegate(),
              GetAllForeignSessions(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](
              std::vector<const sync_sessions::SyncedSession*>* sessions) {
            *sessions = sample_sessions;
            return true;
          }));

  std::vector<history::mojom::SessionPtr> sessions_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&sessions_mojom](
              std::vector<history::mojom::SessionPtr> sessions_arg) {
            sessions_mojom = std::move(sessions_arg);
          }));
  handler().GetTabs(callback.Get());

  ASSERT_EQ(3u, sessions_mojom.size());

  for (unsigned int i = 0; i < kSampleSessionsCount; i++) {
    const auto& session_mojom = sessions_mojom[i];
    ASSERT_TRUE(session_mojom);
    ASSERT_EQ("Test Tag " + base::NumberToString(i), session_mojom->tag);
    ASSERT_EQ(3u, session_mojom->windows.size());
    for (const auto& window : session_mojom->windows) {
      auto tabs = std::move(window->tabs);
      ASSERT_EQ(3u, tabs.size());
      ASSERT_EQ(GURL(kSampleUrl), tabs[0]->url);
    }
  }

  ResetHandler();
}
}  // namespace
