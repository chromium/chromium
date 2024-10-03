// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_request.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/mock_search_picker_client.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/clipboard_history_controller.h"
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

constexpr base::span<const PickerCategory> kAllCategories = {(PickerCategory[]){
    PickerCategory::kEditorWrite,
    PickerCategory::kEditorRewrite,
    PickerCategory::kLinks,
    PickerCategory::kEmojisGifs,
    PickerCategory::kEmojis,
    PickerCategory::kClipboard,
    PickerCategory::kDriveFiles,
    PickerCategory::kLocalFiles,
    PickerCategory::kDatesTimes,
    PickerCategory::kUnitsMaths,
}};

constexpr PickerSearchRequest::Options kDefaultOptions{
    .available_categories = kAllCategories,
    .caps_lock_state_to_search = false,
};

using MockSearchResultsCallback =
    ::testing::MockFunction<PickerSearchRequest::SearchResultsCallback>;

class PickerSearchRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockSearchPickerClient& client() { return client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockSearchPickerClient> client_;
};

TEST_F(PickerSearchRequestTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(Eq(u"cat"), _, _)).Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest,
       DoesNotSendQueryToCrosSearchIfNotAvailableNoCategory) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(_, _, _)).Times(0);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {});
}

TEST_F(PickerSearchRequestTest,
       DoesNotSendQueryToCrosSearchIfNotAvailableWithCategory) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(_, _, _)).Times(0);

  PickerSearchRequest request(
      u"cat", {PickerCategory::kLinks},
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {});
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromOmniboxSearch) {
  MockSearchResultsCallback search_results_callback;
  // Catch-all to prevent unexpected gMock call errors. See
  // https://google.github.io/googletest/gmock_cook_book.html#uninteresting-vs-unexpected
  // for more details.
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kOmnibox,
                   ElementsAre(VariantWith<PickerBrowsingHistoryResult>(
                       Field("url", &PickerBrowsingHistoryResult::url,
                             Property("spec", &GURL::spec,
                                      "https://www.google.com/search?q=cat")))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
}

TEST_F(PickerSearchRequestTest, TruncatesOmniboxResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3"))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerTextResult(u"1"), ash::PickerTextResult(u"2"),
       ash::PickerTextResult(u"3"), ash::PickerTextResult(u"4")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateOmniboxOnlyResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"4"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", PickerCategory::kLinks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerTextResult(u"1"), ash::PickerTextResult(u"2"),
       ash::PickerTextResult(u"3"), ash::PickerTextResult(u"4")});
}

TEST_F(PickerSearchRequestTest, DeduplicatesGoogleCorpGoLinks) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(Ne(PickerSearchSource::kOmnibox), _, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(VariantWith<PickerBrowsingHistoryResult>(
                           Field("url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://example.com"))),
                       VariantWith<PickerBrowsingHistoryResult>(
                           Field("url", &PickerBrowsingHistoryResult::url,
                                 GURL("http://go/link"))),
                       VariantWith<PickerBrowsingHistoryResult>(
                           Field("url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://example.com/2"))),
                       VariantWith<PickerBrowsingHistoryResult>(
                           Field("url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://goto2.corp.google.com/link2"))),
                       VariantWith<PickerBrowsingHistoryResult>(
                           Field("url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://example.com/3")))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", PickerCategory::kLinks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);

  client().cros_search_callback().Run(
      AppListSearchResultType::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"", {}),
          PickerBrowsingHistoryResult(GURL("http://go/link"), u"", {}),
          PickerBrowsingHistoryResult(GURL("https://example.com/2"), u"", {}),
          PickerBrowsingHistoryResult(GURL("https://goto.google.com/link"), u"",
                                      {}),
          PickerBrowsingHistoryResult(
              GURL("https://goto2.corp.google.com/link2"), u"", {}),
          PickerBrowsingHistoryResult(GURL("https://example.com/3"), u"", {}),
          PickerBrowsingHistoryResult(
              GURL("https://goto.corp.google.com/link2"), u"", {}),
      });
}

TEST_F(PickerSearchRequestTest, DoesNotFlashEmptyResultsFromOmniboxSearch) {
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
      .WillByDefault([&search_started, this](
                         const std::u16string& query,
                         std::optional<PickerCategory> category,
                         PickerClient::CrosSearchResultsCallback callback) {
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
  EXPECT_CALL(
      first_search_results_callback,
      Call(PickerSearchSource::kOmnibox, IsEmpty(), /*has_more_results=*/_))
      .Times(0)
      .After(after_start_search_call);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  after_start_search.Call();
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
}

TEST_F(PickerSearchRequestTest, RecordsOmniboxMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.OmniboxProvider.QueryTime", kMetricMetricTime, 1);
}

TEST_F(PickerSearchRequestTest,
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest,
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kFileSearch,
        {ash::PickerTextResult(u"monorail_cat.jpg")});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(
    PickerSearchRequestTest,
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
      .WillByDefault([&search_started, this](
                         const std::u16string& query,
                         std::optional<PickerCategory> category,
                         PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&first_search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 1);
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(VariantWith<PickerTextResult>(Field(
               "text", &PickerTextResult::primary_text, u"monorail_cat.jpg"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});
}

TEST_F(PickerSearchRequestTest, TruncatesResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2.jpg")),

                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3.jpg"))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"1.jpg"), ash::PickerTextResult(u"2.jpg"),
       ash::PickerTextResult(u"3.jpg"), ash::PickerTextResult(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateResultsFromFileOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3.jpg")),

                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"4.jpg"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", PickerCategory::kLocalFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"1.jpg"), ash::PickerTextResult(u"2.jpg"),
       ash::PickerTextResult(u"3.jpg"), ash::PickerTextResult(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, RecordsFileMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.FileProvider.QueryTime",
                                   kMetricMetricTime, 1);
}

TEST_F(PickerSearchRequestTest, DoesNotRecordFileMetricsIfNoFileResponse) {
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest,
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(
          PickerSearchSource::kDrive,
          ElementsAre(VariantWith<PickerTextResult>(Field(
              "text", &PickerTextResult::primary_text, u"catrbug_135117.jpg"))),
          /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"catrbug_135117.jpg")});
}

TEST_F(PickerSearchRequestTest, TruncatesResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kDrive,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2.jpg")),

                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3.jpg"))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"1.jpg"), ash::PickerTextResult(u"2.jpg"),
       ash::PickerTextResult(u"3.jpg"), ash::PickerTextResult(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateResultsFromDriveOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kDrive,
           ElementsAre(VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"1.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"2.jpg")),
                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"3.jpg")),

                       VariantWith<PickerTextResult>(Field(
                           "text", &PickerTextResult::primary_text, u"4.jpg"))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", /*category=*/PickerCategory::kDriveFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"1.jpg"), ash::PickerTextResult(u"2.jpg"),
       ash::PickerTextResult(u"3.jpg"), ash::PickerTextResult(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, RecordsDriveMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"catrbug_135117.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.DriveProvider.QueryTime",
                                   kMetricMetricTime, 1);
}

TEST_F(PickerSearchRequestTest, DoesNotRecordDriveMetricsIfNoDriveResponse) {
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest,
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
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerBrowsingHistoryResult(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest, PublishesDateResultsOnlyOnce) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDate, _, /*has_more_results=*/_))
      .Times(1);
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  PickerSearchRequest request(
      u"next Friday", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest, RecordsDateMetricsOnlyOnce) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  {
    PickerSearchRequest request(
        u"next Friday", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DateProvider.QueryTime", 1);
}

TEST_F(PickerSearchRequestTest, PublishesDateResultsWhenDateCategorySelected) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDate, _, /*has_more_results=*/_))
      .Times(1);
  // Fast forward the clock to a Sunday (day_of_week = 0).
  base::Time::Exploded exploded;
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  task_environment().AdvanceClock(base::Days(7 - exploded.day_of_week));
  task_environment().GetMockClock()->Now().LocalExplode(&exploded);
  ASSERT_EQ(0, exploded.day_of_week);

  PickerSearchRequest request(
      u"next Friday", PickerCategory::kDatesTimes,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest, PublishesMathResultsOnlyOnce) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  PickerSearchRequest request(
      u"1 + 1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest, RecordsMathMetricsOnlyOnce) {
  base::HistogramTester histogram;
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  {
    PickerSearchRequest request(
        u"1 + 1", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        base::DoNothing(), &client(), kDefaultOptions);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.MathProvider.QueryTime", 1);
}

TEST_F(PickerSearchRequestTest, PublishesMathResultsWhenMathCategorySelected) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  PickerSearchRequest request(
      u"1 + 1", PickerCategory::kUnitsMaths,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest, OnlyStartCrosSearchForCertainCategories) {
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"ant"), Eq(PickerCategory::kLinks), _))
      .Times(1);
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"bat"), Eq(PickerCategory::kDriveFiles), _))
      .Times(1);
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"cat"), Eq(PickerCategory::kLocalFiles), _))
      .Times(1);

  {
    PickerSearchRequest request(u"ant", PickerCategory::kLinks,
                                base::DoNothing(), base::DoNothing(), &client(),
                                kDefaultOptions);
  }
  {
    PickerSearchRequest request(u"bat", PickerCategory::kDriveFiles,
                                base::DoNothing(), base::DoNothing(), &client(),
                                kDefaultOptions);
  }
  {
    PickerSearchRequest request(u"cat", PickerCategory::kLocalFiles,
                                base::DoNothing(), base::DoNothing(), &client(),
                                kDefaultOptions);
  }
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromClipboardSearch) {
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
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kClipboard,
                   ElementsAre(VariantWith<PickerClipboardResult>(FieldsAre(
                       _, PickerClipboardResult::DisplayFormat::kText,
                       /*file_count=*/0, u"cat", std::nullopt, true))),
                   /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);
}

TEST_F(PickerSearchRequestTest, RecordsClipboardMetrics) {
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

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), kDefaultOptions);

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.ClipboardProvider.QueryTime", kMetricMetricTime, 1);
}

class PickerSearchRequestEditorTest
    : public PickerSearchRequestTest,
      public testing::WithParamInterface<
          std::pair<PickerCategory, PickerSearchSource>> {};

TEST_P(PickerSearchRequestEditorTest, ShowsResultsFromEditorSearch) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(source, ElementsAre(VariantWith<PickerEditorResult>(_)),
                   /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {.available_categories = {{category}}});
}

TEST_P(PickerSearchRequestEditorTest,
       DoNotShowResultsFromEditorSearchIfNotAvailable) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(source, _, _)).Times(0);

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {});
}

TEST_P(PickerSearchRequestEditorTest, RecordsEditorMetrics) {
  const auto& [category, source] = GetParam();
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {.available_categories = {{category}}});

  histogram.ExpectTotalCount("Ash.Picker.Search.EditorProvider.QueryTime", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerSearchRequestEditorTest,
    testing::Values(std::make_pair(PickerCategory::kEditorWrite,
                                   PickerSearchSource::kEditorWrite),
                    std::make_pair(PickerCategory::kEditorRewrite,
                                   PickerSearchSource::kEditorRewrite)));

class PickerSearchRequestLobsterTest
    : public PickerSearchRequestTest,
      public testing::WithParamInterface<
          std::pair<PickerCategory, PickerSearchSource>> {};

TEST_P(PickerSearchRequestLobsterTest, ShowsResultsFromLobsterSearch) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(source, ElementsAre(VariantWith<PickerLobsterResult>(_)),
                   /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {.available_categories = {{category}}});
}

TEST_P(PickerSearchRequestLobsterTest,
       DoNotShowResultsFromLobsterSearchIfNotAvailable) {
  const auto& [category, source] = GetParam();
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(source, _, _)).Times(0);

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {});
}

TEST_P(PickerSearchRequestLobsterTest, RecordsLobsterMetrics) {
  const auto& [category, source] = GetParam();
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      base::DoNothing(), &client(), {.available_categories = {{category}}});

  histogram.ExpectTotalCount("Ash.Picker.Search.LobsterProvider.QueryTime", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerSearchRequestLobsterTest,
    testing::Values(std::make_pair(PickerCategory::kLobster,
                                   PickerSearchSource::kLobster),
                    std::make_pair(PickerCategory::kLobster,
                                   PickerSearchSource::kLobster)));

TEST_F(PickerSearchRequestTest, DoneClosureCalledImmediatelyWhenNoSearch) {
  // This actually calls category search.
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(), {});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(PickerSearchRequestTest, DoneClosureCalledImmediatelyWhenSynchronous) {
  // This actually calls category search.
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(PickerSearchSource::kAction, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(PickerSearchSource::kMath, _, _))
      .Times(1);
  base::test::TestFuture<bool> done_callback;

  PickerSearchRequest request(
      u"1+1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {.available_categories = {{PickerCategory::kUnitsMaths}}});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(PickerSearchRequestTest, DoneClosureNotCalledWhenAsynchronous) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  // We expect there to be at least one asynchronous source.
  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(), kDefaultOptions);

  EXPECT_FALSE(done_callback.IsReady());
}

TEST_F(PickerSearchRequestTest, DoneClosureCalledAfterClipboard) {
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

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {.available_categories = {{PickerCategory::kClipboard}}});
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

TEST_F(PickerSearchRequestTest, DoneClosureCalledAfterSingleCrosSearchSource) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {.available_categories = {{PickerCategory::kLinks}}});
  EXPECT_FALSE(done_callback.IsReady());
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});

  bool interrupted = done_callback.Get();
  EXPECT_FALSE(interrupted);
}

TEST_F(PickerSearchRequestTest,
       DoneClosureCalledAfterMultipleCrosSearchSources) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {.available_categories = {{PickerCategory::kLinks,
                                 PickerCategory::kDriveFiles,
                                 PickerCategory::kLocalFiles}}});
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

TEST_F(PickerSearchRequestTest, DoneClosureCalledAfterClipboardAndOmnibox) {
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

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.GetCallback(), &client(),
      {.available_categories = {
           {PickerCategory::kClipboard, PickerCategory::kLinks}}});
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

TEST_F(PickerSearchRequestTest,
       DoneClosureCalledAfterSearchCallbackSynchronous) {
  MockSearchResultsCallback search_results_callback;
  base::MockOnceCallback<void(bool)> done_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback, Call(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(done_callback, Run(/*interrupted=*/false)).Times(1);
  }

  PickerSearchRequest request(
      u"1+1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.Get(), &client(),
      {.available_categories = {{PickerCategory::kUnitsMaths}}});
}

TEST_F(PickerSearchRequestTest,
       DoneClosureCalledAfterSearchCallbackAsynchronous) {
  MockSearchResultsCallback search_results_callback;
  base::MockOnceCallback<void(bool)> done_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback, Call(_, _, _)).Times(AtLeast(1));
    EXPECT_CALL(done_callback, Run(/*interrupted*/ false)).Times(1);
  }

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      done_callback.Get(), &client(),
      {.available_categories = {{PickerCategory::kLinks}}});
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox, {});
}

TEST_F(PickerSearchRequestTest, DoneClosureCalledWhenDestructed) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  base::test::TestFuture<bool> done_callback;

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        done_callback.GetCallback(), &client(),
        {.available_categories = {{PickerCategory::kLinks}}});
    EXPECT_FALSE(done_callback.IsReady());
  }

  bool interrupted = done_callback.Get();
  EXPECT_TRUE(interrupted);
}

}  // namespace
}  // namespace ash
