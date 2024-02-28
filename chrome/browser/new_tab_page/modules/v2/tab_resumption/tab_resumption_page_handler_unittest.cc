// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_page_handler.h"

#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
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

using ntp::MockHistoryService;

class TabResumptionPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  TabResumptionPageHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    mock_session_sync_service_ = static_cast<MockSessionSyncService*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
    mock_history_service_ =
        static_cast<MockHistoryService*>(HistoryServiceFactory::GetForProfile(
            profile(), ServiceAccessType::EXPLICIT_ACCESS));
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<TabResumptionPageHandler>(
        mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>(),
        web_contents_.get());
  }

  void TearDown() override {
    handler_.reset();
    web_contents_.reset();
    mock_history_service_ = nullptr;
    mock_session_sync_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  MockHistoryService& mock_history_service() { return *mock_history_service_; }

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
             })},
            {HistoryServiceFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<MockHistoryService>();
             })}};
  }
  raw_ptr<MockHistoryService> mock_history_service_;
  raw_ptr<MockSessionSyncService> mock_session_sync_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<TabResumptionPageHandler> handler_;
};

TEST_F(TabResumptionPageHandlerTest, GetTabs) {
  const size_t kSampleSessionsCount = 2;
  const size_t kSampleTabsCount = 2;
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  std::vector<base::Time> timestamps = {base::Time::Now(),
                                        base::Time::Now() - base::Minutes(1),
                                        base::Time::Now() - base::Minutes(2),
                                        base::Time::Now() - base::Minutes(3)};
  for (size_t i = 0; i < kSampleSessionsCount; i++) {
    sample_sessions.push_back(
        SampleSession(("Test Name " + base::NumberToString(i)).c_str(), 1,
                      kSampleTabsCount, timestamps));
  }

  EXPECT_CALL(*mock_session_sync_service().GetOpenTabsUIDelegate(),
              GetAllForeignSessions(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](
              std::vector<vector_experimental_raw_ptr<
                  const sync_sessions::SyncedSession>>* sessions) {
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

  EXPECT_CALL(mock_history_service(),
              QueryURLs(testing::_, true, testing::_, testing::_))
      .WillOnce(
          (testing::Invoke([&](const std::vector<GURL>& urls, bool want_visits,
                               MockHistoryService::QueryURLsCallback callback,
                               base::CancelableTaskTracker* tracker) {
            std::vector<history::QueryURLResult> results;
            for (auto url : urls) {
              history::QueryURLResult result;
              result.success = true;
              result.row.set_url(url);
              result.row.set_last_visit(base::Time::Now());
              history::VisitVector visits;
              history::VisitRow visit;
              visits.push_back(visit);
              result.visits = visits;
              results.push_back(result);
            }
            std::move(callback).Run(results);
            return base::CancelableTaskTracker::TaskId();
          })));

  EXPECT_CALL(mock_history_service(),
              ToAnnotatedVisits(testing::_, false, testing::_, testing::_))
      .WillOnce((testing::Invoke(
          [&](const history::VisitVector& visit_rows,
              bool compute_redirect_chain_start_properties,
              MockHistoryService::ToAnnotatedVisitsCallback callback,
              base::CancelableTaskTracker* tracker) {
            std::vector<history::AnnotatedVisit> annotated_visits;
            for (auto visit : visit_rows) {
              history::URLRow url_row;
              url_row.set_url(GURL(kSampleUrl));
              history::AnnotatedVisit annotated_visit;
              annotated_visit.url_row = url_row;
              history::VisitContentModelAnnotations model_annotations;
              model_annotations.visibility_score = 1.0;
              history::VisitContentAnnotations content_annotations;
              content_annotations.model_annotations = model_annotations;
              annotated_visit.content_annotations = content_annotations;
              annotated_visits.push_back(annotated_visit);
            }
            std::move(callback).Run(annotated_visits);
            return base::CancelableTaskTracker::TaskId();
          })));

  handler().GetTabs(callback.Get());

  ASSERT_EQ(4u, tabs_mojom.size());

  for (size_t i = 0; i < kSampleSessionsCount * kSampleTabsCount; i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    // As the relative time on the tabs is the tab_id (in minutes) the tabs will
    // be ranked 1 (tab_id = 0), 1 (tab_id = 1), 0 (tab_id = 2), 0 (tab_id = 3)
    // with regard to session_tag.
    ASSERT_EQ(
        "Test Name " + base::NumberToString(
                           ((kSampleSessionsCount * kSampleTabsCount - 1) - i) /
                           kSampleSessionsCount),
        tab_mojom->session_name);
    // Assert that for a tab from 0 minutes ago the displayed text is 'Recently
    // opened'. The first tab after ranking will have be 0 minutes ago.
    if (i == 0) {
      ASSERT_EQ("Recently opened", tab_mojom->relative_time_text);
    }
    ASSERT_EQ(GURL(kSampleUrl), tab_mojom->url);
  }
}

TEST_F(TabResumptionPageHandlerTest, BlocklistTest) {
  const size_t kSampleSessionsCount = 3;
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>> sample_sessions;
  std::vector<base::Time> timestamps = {base::Time::Now(),
                                        base::Time::Now() - base::Minutes(1),
                                        base::Time::Now() - base::Minutes(2),
                                        base::Time::Now() - base::Minutes(3)};
  for (size_t i = 0; i < kSampleSessionsCount; i++) {
    sample_sessions.push_back(SampleSession(
        ("Test Name " + base::NumberToString(i)).c_str(), 1, 1, timestamps));
  }

  EXPECT_CALL(*mock_session_sync_service().GetOpenTabsUIDelegate(),
              GetAllForeignSessions(testing::_))
      .WillOnce(testing::Invoke(
          [&sample_sessions](
              std::vector<vector_experimental_raw_ptr<
                  const sync_sessions::SyncedSession>>* sessions) {
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

  EXPECT_CALL(mock_history_service(),
              QueryURLs(testing::_, true, testing::_, testing::_))
      .WillOnce(
          (testing::Invoke([&](const std::vector<GURL>& urls, bool want_visits,
                               MockHistoryService::QueryURLsCallback callback,
                               base::CancelableTaskTracker* tracker) {
            std::vector<history::QueryURLResult> results;
            for (auto url : urls) {
              history::QueryURLResult result;
              result.success = true;
              result.row.set_url(url);
              result.row.set_last_visit(base::Time::Now());
              history::VisitVector visits;
              history::VisitRow visit;
              visits.push_back(visit);
              result.visits = visits;
              results.push_back(result);
            }
            std::move(callback).Run(results);
            return base::CancelableTaskTracker::TaskId();
          })));

  EXPECT_CALL(mock_history_service(),
              ToAnnotatedVisits(testing::_, false, testing::_, testing::_))
      .WillOnce((testing::Invoke(
          [&](const history::VisitVector& visit_rows,
              bool compute_redirect_chain_start_properties,
              MockHistoryService::ToAnnotatedVisitsCallback callback,
              base::CancelableTaskTracker* tracker) {
            std::vector<history::AnnotatedVisit> annotated_visits;
            for (auto visit : visit_rows) {
              history::URLRow url_row;
              url_row.set_url(GURL(kSampleUrl));
              history::AnnotatedVisit annotated_visit;
              annotated_visit.url_row = url_row;
              history::VisitContentModelAnnotations model_annotations;
              model_annotations.visibility_score = 1.0;
              history::VisitContentAnnotations content_annotations;
              content_annotations.model_annotations = model_annotations;
              annotated_visit.content_annotations = content_annotations;
              annotated_visits.push_back(annotated_visit);
            }
            history::VisitContentModelAnnotations::Category category;
            category.id = "/g/11b76fyj2r";
            annotated_visits[kSampleSessionsCount - 1]
                .content_annotations.model_annotations.categories.push_back(
                    category);
            std::move(callback).Run(annotated_visits);
            return base::CancelableTaskTracker::TaskId();
          })));

  handler().GetTabs(callback.Get());

  // Last visit has a blocked category so it should be excluded
  ASSERT_EQ(2u, tabs_mojom.size());

  for (size_t i = 0; i < kSampleSessionsCount - 1; i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    // Ranking reverses the order due to setting timestamp as
    // reverse order of timestamps array above.
    // Third entry is gone from blocklist so this starts at "Test Name 1".
    ASSERT_EQ("Test Name " + base::NumberToString(kSampleSessionsCount - i - 2),
              tab_mojom->session_name);
    ASSERT_EQ(GURL(kSampleUrl), tab_mojom->url);
  }
}
}  // namespace
