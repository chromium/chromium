// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/model/quick_insert_link_suggester.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"

namespace ash {
namespace {

using ::testing::Field;

class TestFaviconService : public favicon::MockFaviconService {
 public:
  TestFaviconService() = default;
  TestFaviconService(const TestFaviconService&) = delete;
  TestFaviconService& operator=(const TestFaviconService&) = delete;
  ~TestFaviconService() override = default;

  // favicon::FaviconService:
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    page_url_ = page_url;
    std::move(callback).Run(favicon_base::FaviconImageResult());
    return {};
  }

  GURL page_url_;
};

class QuickInsertLinkSuggesterTest : public testing::Test {
 public:
  QuickInsertLinkSuggesterTest() {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ =
        history::CreateHistoryService(history_dir_.GetPath(), true);
  }

  history::HistoryService* GetHistoryService() {
    return history_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
};

TEST_F(QuickInsertLinkSuggesterTest, GetSuggestedLinkResultsReturnsLinks) {
  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();
  history_service->AddPageWithDetails(
      GURL("http://a.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("http://b.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  TestFaviconService favicon_service;
  QuickInsertLinkSuggester link_suggester;

  base::test::TestFuture<std::vector<ash::QuickInsertSearchResult>> future;
  link_suggester.GetSuggestedLinks(history_service, &favicon_service, 100u,
                                   future.GetRepeatingCallback());

  EXPECT_THAT(
      future.Get(),
      ElementsAre(VariantWith<ash::QuickInsertBrowsingHistoryResult>(
                      Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                            GURL("http://b.com"))),
                  VariantWith<ash::QuickInsertBrowsingHistoryResult>(
                      Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                            GURL("http://a.com")))));
  EXPECT_EQ(favicon_service.page_url_, GURL("http://a.com"));
}

TEST_F(QuickInsertLinkSuggesterTest,
       GetSuggestedLinkResultsAreTruncatedToMostRecent) {
  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();
  history_service->AddPageWithDetails(
      GURL("http://a.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("http://b.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  TestFaviconService favicon_service;
  QuickInsertLinkSuggester link_suggester;

  base::test::TestFuture<std::vector<ash::QuickInsertSearchResult>> future;
  link_suggester.GetSuggestedLinks(history_service, &favicon_service, 1u,
                                   future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::QuickInsertBrowsingHistoryResult>(
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("http://b.com")))));
  EXPECT_EQ(favicon_service.page_url_, GURL("http://b.com"));
}

TEST_F(QuickInsertLinkSuggesterTest, GetSuggestedLinkResultsExclude404s) {
  // Allow saving 404 visits to History.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(history::kVisitedLinksOn404);

  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();

  // Add a non-404 visit.
  history_service->AddPageWithDetails(
      GURL("http://a.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1, /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);

  // Add a 404 visit.
  history::HistoryAddPageArgs page_404_args;
  page_404_args.url = GURL("http://b.com/404");
  page_404_args.time = now;
  page_404_args.hidden = true;
  page_404_args.context_annotations =
      std::make_optional<history::VisitContextAnnotations::OnVisitFields>(
          {.response_code = 404});
  page_404_args.response_code_category =
      history::VisitResponseCodeCategory::k404;
  history_service->AddPage(page_404_args);

  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  TestFaviconService favicon_service;
  QuickInsertLinkSuggester link_suggester;

  base::test::TestFuture<std::vector<ash::QuickInsertSearchResult>> future;
  link_suggester.GetSuggestedLinks(history_service, &favicon_service, 100u,
                                   future.GetRepeatingCallback());

  // We should only get the non-404 result back.
  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::QuickInsertBrowsingHistoryResult>(
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("http://a.com")))));
}

TEST_F(QuickInsertLinkSuggesterTest,
       GetSuggestedLinkResultsFiltersOutPersonalizedLinks) {
  const base::Time now = base::Time::Now();
  auto* history_service = GetHistoryService();
  history_service->AddPageWithDetails(
      GURL("https://mail.google.com/mail/u/0/#inbox/aaa"), /*title=*/u"",
      /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now - base::Seconds(1),
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("https://mail.google.com/chat/u/0/#chat/aaa"), /*title=*/u"",
      /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history_service->AddPageWithDetails(
      GURL("https://mail.google.com"), /*title=*/u"", /*visit_count=*/1,
      /*typed_count=*/1,
      /*last_visit=*/now,
      /*hidden=*/false, history::SOURCE_BROWSED);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
  QuickInsertLinkSuggester link_suggester;

  base::test::TestFuture<std::vector<ash::QuickInsertSearchResult>> future;
  link_suggester.GetSuggestedLinks(history_service,
                                   /*favicon_service=*/nullptr, 100u,
                                   future.GetRepeatingCallback());

  EXPECT_THAT(future.Get(),
              ElementsAre(VariantWith<ash::QuickInsertBrowsingHistoryResult>(
                  Field("url", &ash::QuickInsertBrowsingHistoryResult::url,
                        GURL("https://mail.google.com")))));
}

}  // namespace
}  // namespace ash
