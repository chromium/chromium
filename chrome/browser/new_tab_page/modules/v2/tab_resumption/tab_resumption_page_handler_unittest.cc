// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption_page_handler.h"

#include <string>
#include <vector>

#include "base/task/cancelable_task_tracker.h"
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

base::CancelableTaskTracker::TaskId MockGetMostRecentVisitForEachURL(
    const std::vector<GURL>& urls,
    base::OnceCallback<void(std::map<GURL, history::VisitRow>)> callback,
    base::CancelableTaskTracker* tracker) {
  std::map<GURL, history::VisitRow> visits;
  for (auto url : urls) {
    history::VisitRow visit;
    visits[url] = visit;
  }
  std::move(callback).Run(visits);
  return base::CancelableTaskTracker::TaskId();
}

base::CancelableTaskTracker::TaskId MockToAnnotatedVisits(
    const history::VisitVector& visit_rows,
    bool compute_redirect_chain_start_properties,
    MockHistoryService::ToAnnotatedVisitsCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::vector<history::AnnotatedVisit> annotated_visits;
  for (size_t i = 0; i < visit_rows.size(); i++) {
    history::URLRow url_row;
    url_row.set_url(GURL(kSampleUrl + (i != 0 ? base::NumberToString(i) : "")));
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
}

base::CancelableTaskTracker::TaskId MockToAnnotatedVisitsBlocklist(
    const history::VisitVector& visit_rows,
    bool compute_redirect_chain_start_properties,
    MockHistoryService::ToAnnotatedVisitsCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::vector<history::AnnotatedVisit> annotated_visits;
  for (size_t i = 0; i < visit_rows.size(); i++) {
    history::URLRow url_row;
    size_t url_suffix = visit_rows.size() - i - 1;
    url_row.set_url(
        GURL(kSampleUrl +
             (url_suffix != 0 ? base::NumberToString(url_suffix) : "")));
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
  annotated_visits[visit_rows.size() - 1]
      .content_annotations.model_annotations.categories.push_back(category);
  std::move(callback).Run(annotated_visits);
  return base::CancelableTaskTracker::TaskId();
}

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

  void SetUpMockCalls(
      const std::vector<std::unique_ptr<sync_sessions::SyncedSession>>&
          sample_sessions,
      std::vector<history::mojom::TabPtr>& tabs_mojom,
      base::MockCallback<TabResumptionPageHandler::GetTabsCallback>& callback,
      bool use_blocklist = false) {
    EXPECT_CALL(*mock_session_sync_service().GetOpenTabsUIDelegate(),
                GetAllForeignSessions(testing::_))
        .WillOnce(testing::Invoke(
            [&sample_sessions](
                std::vector<raw_ptr<const sync_sessions::SyncedSession,
                                    VectorExperimental>>* sessions) {
              for (auto& sample_session : sample_sessions) {
                sessions->push_back(sample_session.get());
              }
              return true;
            }));

    EXPECT_CALL(mock_history_service(), GetMostRecentVisitForEachURL(
                                            testing::_, testing::_, testing::_))
        .WillOnce((testing::Invoke(&MockGetMostRecentVisitForEachURL)));

    EXPECT_CALL(mock_history_service(),
                ToAnnotatedVisits(testing::_, false, testing::_, testing::_))
        .WillOnce(
            (testing::Invoke(use_blocklist ? &MockToAnnotatedVisitsBlocklist
                                           : &MockToAnnotatedVisits)));

    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce(testing::Invoke(
            [&tabs_mojom](std::vector<history::mojom::TabPtr> tabs_arg) {
              tabs_mojom = std::move(tabs_arg);
            }));
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
    return {TestingProfile::TestingFactory{
                SessionSyncServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockSessionSyncService>();
                })},
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
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
  const size_t kSampleSessionsCount = 3;
  const size_t kSampleTabsCount = 1;
  std::vector<base::Time> timestamps = {base::Time::Now() - base::Minutes(2),
                                        base::Time::Now() - base::Minutes(1),
                                        base::Time::Now()};
  const auto sample_sessions = SampleSessions(
      kSampleSessionsCount, kSampleTabsCount, std::move(timestamps));
  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;

  SetUpMockCalls(sample_sessions, tabs_mojom, callback);

  handler().GetTabs(callback.Get());

  ASSERT_EQ(3u, tabs_mojom.size());

  // As the relative time on the tabs is the tab_id (in minutes) the tabs will
  // be ranked 1 (tab_id = 0), 0 (tab_id = 1), 0 (tab_id = 2), 0 (tab_id = 3)
  // with regard to session_tag. As there are duplicate tabs now the most recent
  // one (by a microseconds difference) will be the first tab (Test Name 2).
  std::vector<std::string> session_names = {"Test Name 0", "Test Name 1",
                                            "Test Name 2"};

  for (size_t i = 0; i < tabs_mojom.size(); i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    ASSERT_EQ(session_names[i], tab_mojom->session_name);
    // Assert that for a tab from 0 minutes ago the displayed text is 'Recently
    // opened'. The first tab after ranking will have be 0 minutes ago.
    if (i == 0) {
      ASSERT_EQ("Recently opened", tab_mojom->relative_time_text);
    }
    ASSERT_EQ(GURL(kSampleUrl + (i != 0 ? base::NumberToString(i) : "")),
              tab_mojom->url);
  }
}

TEST_F(TabResumptionPageHandlerTest, SortTabs) {
  const size_t kSampleSessionsCount = 4;
  const size_t kSampleTabsCount = 1;
  std::vector<base::Time> timestamps = {base::Time::Now(),
                                        base::Time::Now() - base::Minutes(1),
                                        base::Time::Now() - base::Minutes(2),
                                        base::Time::Now() - base::Minutes(3)};
  const auto sample_sessions = SampleSessions(
      kSampleSessionsCount, kSampleTabsCount, std::move(timestamps));
  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;

  SetUpMockCalls(sample_sessions, tabs_mojom, callback);

  handler().GetTabs(callback.Get());

  ASSERT_EQ(4u, tabs_mojom.size());

  // As the tab names correspond to number of tabs minus relative time (in
  // minutes) (i.e. Test name 3 would correspond to relative time 0), the test
  // names will all be in reverse order.
  std::vector<std::string> session_names = {"Test Name 3", "Test Name 2",
                                            "Test Name 1", "Test Name 0"};

  for (size_t i = 0; i < tabs_mojom.size(); i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    ASSERT_EQ(session_names[i], tab_mojom->session_name);
    // Assert that for a tab from 0 minutes ago the displayed text is 'Recently
    // opened'. The first tab after ranking will have be 0 minutes ago.
    if (i == 0) {
      ASSERT_EQ("Recently opened", tab_mojom->relative_time_text);
    }
    ASSERT_EQ(GURL(kSampleUrl + (i != 0 ? base::NumberToString(i) : "")),
              tab_mojom->url);
  }
}

TEST_F(TabResumptionPageHandlerTest, DeduplicateTabs) {
  const size_t kSampleSessionsCount = 1;
  const size_t kSampleTabsCount = 3;
  std::vector<base::Time> timestamps = {};
  const auto sample_sessions = SampleSessions(
      kSampleSessionsCount, kSampleTabsCount, std::move(timestamps));
  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;

  SetUpMockCalls(sample_sessions, tabs_mojom, callback);

  handler().GetTabs(callback.Get());

  ASSERT_EQ(1u, tabs_mojom.size());
  // As there are duplicate tabs now only one will show
  const auto& tab_mojom = tabs_mojom[0];
  ASSERT_TRUE(tab_mojom);
  ASSERT_EQ("Test Name 0", tab_mojom->session_name);
  // Assert that for a tab from 0 minutes ago the displayed text is 'Recently
  // opened'. All the tabs are have a timestamp of 0 minutes ago.
  ASSERT_EQ("Recently opened", tab_mojom->relative_time_text);
  ASSERT_EQ(GURL(kSampleUrl), tab_mojom->url);
}

TEST_F(TabResumptionPageHandlerTest, BlocklistTest) {
  const size_t kSampleSessionsCount = 3;
  const size_t kSampleTabsCount = 1;
  std::vector<base::Time> timestamps = {base::Time::Now() - base::Minutes(2),
                                        base::Time::Now() - base::Minutes(1),
                                        base::Time::Now()};
  const auto sample_sessions = SampleSessions(
      kSampleSessionsCount, kSampleTabsCount, std::move(timestamps));

  std::vector<history::mojom::TabPtr> tabs_mojom;
  base::MockCallback<TabResumptionPageHandler::GetTabsCallback> callback;

  SetUpMockCalls(sample_sessions, tabs_mojom, callback, true);

  handler().GetTabs(callback.Get());

  // Last visit has a blocked category so it should be excluded
  ASSERT_EQ(2u, tabs_mojom.size());

  std::vector<std::string> session_names = {"Test Name 1", "Test Name 2"};
  std::vector<std::string> session_urls = {kSampleUrl + std::string("1"),
                                           kSampleUrl + std::string("2")};

  for (size_t i = 0; i < kSampleSessionsCount - 1; i++) {
    const auto& tab_mojom = tabs_mojom[i];
    ASSERT_TRUE(tab_mojom);
    // Third entry is gone from blocklist so this ends at "Test Name 2"
    // and starts at "Test Name 1".
    ASSERT_EQ(session_names[i], tab_mojom->session_name);
    ASSERT_EQ(GURL(session_urls[i]), tab_mojom->url);
  }
}
}  // namespace
