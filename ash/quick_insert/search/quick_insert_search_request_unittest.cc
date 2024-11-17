// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_request.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/mock_search_quick_insert_client.h"
#include "ash/quick_insert/search/quick_insert_search_request.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

constexpr base::TimeDelta kMetricMetricTime = base::Milliseconds(300);

constexpr auto kAllCategories = std::to_array({
    QuickInsertCategory::kEditorWrite,
    QuickInsertCategory::kEditorRewrite,
    QuickInsertCategory::kLinks,
    QuickInsertCategory::kEmojisGifs,
    QuickInsertCategory::kEmojis,
    QuickInsertCategory::kClipboard,
    QuickInsertCategory::kDriveFiles,
    QuickInsertCategory::kLocalFiles,
    QuickInsertCategory::kDatesTimes,
    QuickInsertCategory::kUnitsMaths,
});

using MockSearchResultsCallback =
    ::testing::MockFunction<QuickInsertSearchRequest::SearchResultsCallback>;

class QuickInsertSearchRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockSearchQuickInsertClient& client() { return client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockSearchQuickInsertClient> client_;
};

TEST_F(QuickInsertSearchRequestTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(Eq(u"cat"), _, _)).Times(1);

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotSendQueryToCrosSearchIfNotAvailableNoCategory) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(_, _, _)).Times(0);

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client());
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotSendQueryToCrosSearchIfNotAvailableWithCategory) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(_, _, _)).Times(0);

  QuickInsertSearchRequest request(
      u"cat", {QuickInsertCategory::kLinks},
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client());
}

TEST_F(QuickInsertSearchRequestTest, ShowsResultsFromOmniboxSearch) {
  MockSearchResultsCallback search_results_callback;
  // Catch-all to prevent unexpected gMock call errors. See
  // https://google.github.io/googletest/gmock_cook_book.html#uninteresting-vs-unexpected
  // for more details.
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kOmnibox,
                   ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(
                       Field("url", &QuickInsertBrowsingHistoryResult::url,
                             Property("spec", &GURL::spec,
                                      "https://www.google.com/search?q=cat")))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::QuickInsertBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
}

TEST_F(QuickInsertSearchRequestTest, TruncatesOmniboxResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(
          QuickInsertSearchSource::kOmnibox,
          ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"1")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"2")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"3"))),
          /*has_more_results=*/true))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::QuickInsertTextResult(u"1"), ash::QuickInsertTextResult(u"2"),
       ash::QuickInsertTextResult(u"3"), ash::QuickInsertTextResult(u"4")});
}

TEST_F(QuickInsertSearchRequestTest, DoesNotTruncateOmniboxOnlyResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(
          QuickInsertSearchSource::kOmnibox,
          ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"1")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"2")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"3")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "text", &QuickInsertTextResult::primary_text, u"4"))),
          /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", QuickInsertCategory::kLinks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::QuickInsertTextResult(u"1"), ash::QuickInsertTextResult(u"2"),
       ash::QuickInsertTextResult(u"3"), ash::QuickInsertTextResult(u"4")});
}

TEST_F(QuickInsertSearchRequestTest, DeduplicatesGoogleCorpGoLinks) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(Ne(QuickInsertSearchSource::kOmnibox), _, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kOmnibox,
           ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(
                           Field("url", &QuickInsertBrowsingHistoryResult::url,
                                 GURL("https://example.com"))),
                       VariantWith<QuickInsertBrowsingHistoryResult>(
                           Field("url", &QuickInsertBrowsingHistoryResult::url,
                                 GURL("http://go/link"))),
                       VariantWith<QuickInsertBrowsingHistoryResult>(
                           Field("url", &QuickInsertBrowsingHistoryResult::url,
                                 GURL("https://example.com/2"))),
                       VariantWith<QuickInsertBrowsingHistoryResult>(
                           Field("url", &QuickInsertBrowsingHistoryResult::url,
                                 GURL("https://goto2.corp.google.com/link2"))),
                       VariantWith<QuickInsertBrowsingHistoryResult>(
                           Field("url", &QuickInsertBrowsingHistoryResult::url,
                                 GURL("https://example.com/3")))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", QuickInsertCategory::kLinks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);

  client().cros_search_callback().Run(
      AppListSearchResultType::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           {}),
          QuickInsertBrowsingHistoryResult(GURL("http://go/link"), u"", {}),
          QuickInsertBrowsingHistoryResult(GURL("https://example.com/2"), u"",
                                           {}),
          QuickInsertBrowsingHistoryResult(GURL("https://goto.google.com/link"),
                                           u"", {}),
          QuickInsertBrowsingHistoryResult(
              GURL("https://goto2.corp.google.com/link2"), u"", {}),
          QuickInsertBrowsingHistoryResult(GURL("https://example.com/3"), u"",
                                           {}),
          QuickInsertBrowsingHistoryResult(
              GURL("https://goto.corp.google.com/link2"), u"", {}),
      });
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotFlashEmptyResultsFromOmniboxSearch) {
  NiceMock<MockSearchResultsCallback> first_search_results_callback;
  NiceMock<MockSearchResultsCallback> second_search_results_callback;
  // CrOS search calls `StopSearch()` automatically on starting a search.
  // If `StopSearch` actually stops a search, some providers such as the omnibox
  // automatically call the search result callback from the _last_ search with
  // an empty vector.
  // Ensure that we don't flash empty results if this happens - i.e. that we
  // call `StopSearch` before starting a new search, and calling `StopSearch`
  // does not trigger a search callback call with empty CrOS search results.
  bool search_started = false;
  ON_CALL(client(), StopCrosQuery).WillByDefault([&search_started, this]() {
    if (search_started) {
      client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                          {});
    }
    search_started = false;
  });
  ON_CALL(client(), StartCrosSearch)
      .WillByDefault(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });
  // Function only used for the below `EXPECT_CALL` to ensure that we don't call
  // the search callback with an empty callback after the initial state.
  testing::MockFunction<void()> after_start_search;
  testing::Expectation after_start_search_call =
      EXPECT_CALL(after_start_search, Call).Times(1);
  EXPECT_CALL(first_search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(first_search_results_callback,
              Call(QuickInsertSearchSource::kOmnibox, IsEmpty(),
                   /*has_more_results=*/_))
      .Times(0)
      .After(after_start_search_call);

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  after_start_search.Call();
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::QuickInsertBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
}

TEST_F(QuickInsertSearchRequestTest, RecordsOmniboxMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::QuickInsertBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.OmniboxProvider.QueryTime", kMetricMetricTime, 1);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotRecordOmniboxMetricsIfNoOmniboxResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotRecordOmniboxMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kFileSearch,
        {ash::QuickInsertTextResult(u"monorail_cat.jpg")});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(
    QuickInsertSearchRequestTest,
    DoesNotRecordOmniboxMetricsTwiceIfSearchResultsArePublishedAfterStopSearch) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> first_search_results_callback;
  NiceMock<MockSearchResultsCallback> second_search_results_callback;
  // CrOS search calls `StopSearch()` automatically on starting a search.
  // If `StopSearch` actually stops a search, some providers such as the omnibox
  // automatically call the search result callback from the _last_ search with
  // an empty vector.
  // Ensure that we don't record metrics twice if this happens.
  bool search_started = false;
  ON_CALL(client(), StopCrosQuery).WillByDefault([&search_started, this]() {
    if (search_started) {
      client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                          {});
    }
    search_started = false;
  });
  ON_CALL(client(), StartCrosSearch)
      .WillByDefault(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&first_search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::QuickInsertBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 1);
}

TEST_F(QuickInsertSearchRequestTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kLocalFile,
                   ElementsAre(VariantWith<QuickInsertTextResult>(
                       Field("text", &QuickInsertTextResult::primary_text,
                             u"monorail_cat.jpg"))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::QuickInsertTextResult(u"monorail_cat.jpg")});
}

TEST_F(QuickInsertSearchRequestTest, TruncatesResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kLocalFile,
           ElementsAre(
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"1.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"2.jpg")),

               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"3.jpg"))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(ash::AppListSearchResultType::kFileSearch,
                                      {ash::QuickInsertTextResult(u"1.jpg"),
                                       ash::QuickInsertTextResult(u"2.jpg"),
                                       ash::QuickInsertTextResult(u"3.jpg"),
                                       ash::QuickInsertTextResult(u"4.jpg")});
}

TEST_F(QuickInsertSearchRequestTest, DoesNotTruncateResultsFromFileOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kLocalFile,
           ElementsAre(
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"1.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"2.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"3.jpg")),

               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"4.jpg"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", QuickInsertCategory::kLocalFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(ash::AppListSearchResultType::kFileSearch,
                                      {ash::QuickInsertTextResult(u"1.jpg"),
                                       ash::QuickInsertTextResult(u"2.jpg"),
                                       ash::QuickInsertTextResult(u"3.jpg"),
                                       ash::QuickInsertTextResult(u"4.jpg")});
}

TEST_F(QuickInsertSearchRequestTest, RecordsFileMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::QuickInsertTextResult(u"monorail_cat.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.FileProvider.QueryTime",
                                   kMetricMetricTime, 1);
}

TEST_F(QuickInsertSearchRequestTest, DoesNotRecordFileMetricsIfNoFileResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotRecordFileMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::QuickInsertBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(QuickInsertSearchRequestTest, ShowsResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kDrive,
                   ElementsAre(VariantWith<QuickInsertTextResult>(
                       Field("text", &QuickInsertTextResult::primary_text,
                             u"catrbug_135117.jpg"))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::QuickInsertTextResult(u"catrbug_135117.jpg")});
}

TEST_F(QuickInsertSearchRequestTest, TruncatesResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kDrive,
           ElementsAre(
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"1.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"2.jpg")),

               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"3.jpg"))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::QuickInsertTextResult(u"1.jpg"),
       ash::QuickInsertTextResult(u"2.jpg"),
       ash::QuickInsertTextResult(u"3.jpg"),
       ash::QuickInsertTextResult(u"4.jpg")});
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotTruncateResultsFromDriveOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kDrive,
           ElementsAre(
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"1.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"2.jpg")),
               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"3.jpg")),

               VariantWith<QuickInsertTextResult>(Field(
                   "text", &QuickInsertTextResult::primary_text, u"4.jpg"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  QuickInsertSearchRequest request(
      u"cat", /*category=*/QuickInsertCategory::kDriveFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::QuickInsertTextResult(u"1.jpg"),
       ash::QuickInsertTextResult(u"2.jpg"),
       ash::QuickInsertTextResult(u"3.jpg"),
       ash::QuickInsertTextResult(u"4.jpg")});
}

TEST_F(QuickInsertSearchRequestTest, RecordsDriveMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::QuickInsertTextResult(u"catrbug_135117.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.DriveProvider.QueryTime",
                                   kMetricMetricTime, 1);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotRecordDriveMetricsIfNoDriveResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(QuickInsertSearchRequestTest,
       DoesNotRecordDriveMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(1))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(1)
      .WillRepeatedly(
          [&search_started, this](
              const std::u16string& query,
              std::optional<QuickInsertCategory> category,
              QuickInsertClient::CrosSearchResultsCallback callback) {
            client().StopCrosQuery();
            search_started = true;
            client().cros_search_callback() = std::move(callback);
          });

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::QuickInsertBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(QuickInsertSearchRequestTest, PublishesDateResultsOnlyOnce) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kDate, _, /*has_more_results=*/_))
      .Times(1);
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  QuickInsertSearchRequest request(
      u"next Friday", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest, RecordsDateMetricsOnlyOnce) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  {
    QuickInsertSearchRequest request(
        u"next Friday", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DateProvider.QueryTime", 1);
}

TEST_F(QuickInsertSearchRequestTest,
       PublishesDateResultsWhenDateCategorySelected) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kDate, _, /*has_more_results=*/_))
      .Times(1);
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  QuickInsertSearchRequest request(
      u"next Friday", QuickInsertCategory::kDatesTimes,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest, PublishesMathResultsOnlyOnce) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  QuickInsertSearchRequest request(
      u"1 + 1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest, RecordsMathMetricsOnlyOnce) {
  base::HistogramTester histogram;
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  {
    QuickInsertSearchRequest request(
        u"1 + 1", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.MathProvider.QueryTime", 1);
}

TEST_F(QuickInsertSearchRequestTest,
       PublishesMathResultsWhenMathCategorySelected) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  QuickInsertSearchRequest request(
      u"1 + 1", QuickInsertCategory::kUnitsMaths,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest, OnlyStartCrosSearchForCertainCategories) {
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"ant"), Eq(QuickInsertCategory::kLinks), _))
      .Times(1);
  EXPECT_CALL(
      client(),
      StartCrosSearch(Eq(u"bat"), Eq(QuickInsertCategory::kDriveFiles), _))
      .Times(1);
  EXPECT_CALL(
      client(),
      StartCrosSearch(Eq(u"cat"), Eq(QuickInsertCategory::kLocalFiles), _))
      .Times(1);

  {
    QuickInsertSearchRequest request(u"ant", QuickInsertCategory::kLinks,
                                     base::DoNothing(), base::DoNothing(),
                                     &client(), kAllCategories);
  }
  {
    QuickInsertSearchRequest request(u"bat", QuickInsertCategory::kDriveFiles,
                                     base::DoNothing(), base::DoNothing(),
                                     &client(), kAllCategories);
  }
  {
    QuickInsertSearchRequest request(u"cat", QuickInsertCategory::kLocalFiles,
                                     base::DoNothing(), base::DoNothing(),
                                     &client(), kAllCategories);
  }
}

TEST_F(QuickInsertSearchRequestTest, ShowsResultsFromClipboardSearch) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            std::move(callback).Run(
                {builder.SetFormat(ui::ClipboardInternalFormat::kText)
                     .SetText("cat")
                     .Build()});
          });

  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(QuickInsertSearchSource::kClipboard,
           ElementsAre(VariantWith<QuickInsertClipboardResult>(
               FieldsAre(_, QuickInsertClipboardResult::DisplayFormat::kText,
                         /*file_count=*/0, u"cat", std::nullopt, true))),
           /*has_more_results=*/false))
      .Times(1);

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);
}

TEST_F(QuickInsertSearchRequestTest, RecordsClipboardMetrics) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [this](
              ClipboardHistoryController::GetHistoryValuesCallback callback) {
            task_environment().FastForwardBy(kMetricMetricTime);
            std::move(callback).Run({});
          });
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kAllCategories);

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.ClipboardProvider.QueryTime", kMetricMetricTime, 1);
}

class QuickInsertSearchRequestEditorTest
    : public QuickInsertSearchRequestTest,
      public testing::WithParamInterface<
          std::pair<QuickInsertCategory, QuickInsertSearchSource>> {};

TEST_P(QuickInsertSearchRequestEditorTest, ShowsResultsFromEditorSearch) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(source, ElementsAre(VariantWith<QuickInsertEditorResult>(_)),
                   /*has_more_results=*/false))
      .Times(1);

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), base::span_from_ref(category));
}

TEST_P(QuickInsertSearchRequestEditorTest,
       DoNotShowResultsFromEditorSearchIfNotAvailable) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(source, _, _)).Times(0);

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client());
}

TEST_P(QuickInsertSearchRequestEditorTest, RecordsEditorMetrics) {
  const auto& [category, source] = GetParam();
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), base::span_from_ref(category));

  histogram.ExpectTotalCount("Ash.Picker.Search.EditorProvider.QueryTime", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertSearchRequestEditorTest,
    testing::Values(std::make_pair(QuickInsertCategory::kEditorWrite,
                                   QuickInsertSearchSource::kEditorWrite),
                    std::make_pair(QuickInsertCategory::kEditorRewrite,
                                   QuickInsertSearchSource::kEditorRewrite)));

class QuickInsertSearchRequestLobsterTest
    : public QuickInsertSearchRequestTest,
      public testing::WithParamInterface<
          std::pair<QuickInsertCategory, QuickInsertSearchSource>> {};

TEST_P(QuickInsertSearchRequestLobsterTest, ShowsResultsFromLobsterSearch) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(source, ElementsAre(VariantWith<QuickInsertLobsterResult>(_)),
           /*has_more_results=*/false))
      .Times(1);

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), base::span_from_ref(category));
}

TEST_P(QuickInsertSearchRequestLobsterTest,
       DoNotShowResultsFromLobsterSearchIfNotAvailable) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(source, _, _)).Times(0);

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client());
}

TEST_P(QuickInsertSearchRequestLobsterTest, RecordsLobsterMetrics) {
  const auto& [category, source] = GetParam();
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  QuickInsertSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), base::span_from_ref(category));

  histogram.ExpectTotalCount("Ash.Picker.Search.LobsterProvider.QueryTime", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertSearchRequestLobsterTest,
    testing::Values(
        std::make_pair(QuickInsertCategory::kLobsterWithNoSelectedText,
                       QuickInsertSearchSource::kLobsterWithNoSelectedText),
        std::make_pair(QuickInsertCategory::kLobsterWithSelectedText,
                       QuickInsertSearchSource::kLobsterWithSelectedText)));

TEST_F(QuickInsertSearchRequestTest, DoneClosureCalledImmediatelyWhenNoSearch) {
  // This actually calls category search.
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client());

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledImmediatelyWhenSynchronous) {
  // This actually calls category search.
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kAction, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(QuickInsertSearchSource::kMath, _, _))
      .Times(1);
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"1+1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      base::span_from_ref(QuickInsertCategory::kUnitsMaths));

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest, DoneClosureNotCalledWhenAsynchronous) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  // We expect there to be at least one asynchronous source.
  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(), kAllCategories);

  EXPECT_FALSE(done_callback.IsReady());
}

TEST_F(QuickInsertSearchRequestTest, DoneClosureCalledAfterClipboard) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  base::test::TestFuture<ClipboardHistoryController::GetHistoryValuesCallback>
      get_history_values_future;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            get_history_values_future.SetValue(std::move(callback));
          });
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      base::span_from_ref(QuickInsertCategory::kClipboard));
  EXPECT_FALSE(done_callback.IsReady());
  ClipboardHistoryController::GetHistoryValuesCallback get_history_values =
      get_history_values_future.Take();
  ClipboardHistoryItemBuilder builder;
  std::move(get_history_values)
      .Run({builder.SetFormat(ui::ClipboardInternalFormat::kText)
                .SetText("cat")
                .Build()});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledAfterSingleCrosSearchSource) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      base::span_from_ref(QuickInsertCategory::kLinks));
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledAfterMultipleCrosSearchSources) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {QuickInsertCategory::kLinks, QuickInsertCategory::kDriveFiles,
       QuickInsertCategory::kLocalFiles});
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kDriveSearch,
                                      {});
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kFileSearch, {});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledAfterClipboardAndOmnibox) {
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  base::test::TestFuture<ClipboardHistoryController::GetHistoryValuesCallback>
      get_history_values_future;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&](ClipboardHistoryController::GetHistoryValuesCallback callback) {
            get_history_values_future.SetValue(std::move(callback));
          });
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {QuickInsertCategory::kClipboard, QuickInsertCategory::kLinks});
  EXPECT_FALSE(done_callback.IsReady());
  ClipboardHistoryController::GetHistoryValuesCallback get_history_values =
      get_history_values_future.Take();
  ClipboardHistoryItemBuilder builder;
  std::move(get_history_values)
      .Run({builder.SetFormat(ui::ClipboardInternalFormat::kText)
                .SetText("cat")
                .Build()});
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledAfterSearchCallbackSynchronous) {
  MockSearchResultsCallback search_results_callback;
  base::MockOnceCallback<void(bool)> done_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback, Call(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(done_callback, Run(/*interrupted=*/false)).Times(1);
  }

  QuickInsertSearchRequest request(
      u"1+1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.Get(), &client(),
      base::span_from_ref(QuickInsertCategory::kUnitsMaths));
}

TEST_F(QuickInsertSearchRequestTest,
       DoneClosureCalledAfterSearchCallbackAsynchronous) {
  MockSearchResultsCallback search_results_callback;
  base::MockOnceCallback<void(bool)> done_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback, Call(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(done_callback, Run(/*interrupted*/ false)).Times(1);
  }

  QuickInsertSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.Get(), &client(),
      base::span_from_ref(QuickInsertCategory::kLinks));
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});
}

TEST_F(QuickInsertSearchRequestTest, DoneClosureCalledWhenDestructed) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  {
    QuickInsertSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        done_callback.GetCallback(), &client(),
        base::span_from_ref(QuickInsertCategory::kLinks));
    EXPECT_FALSE(done_callback.IsReady());
  }

  bool interrupted = done_callback.Get();
  EXPECT_TRUE(interrupted);
}

}  // namespace
}  // namespace ash
