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
#include "ash/picker/search/mock_search_picker_client.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
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
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Property;
using ::testing::VariantWith;

constexpr base::TimeDelta kMetricMetricTime = base::Milliseconds(300);

constexpr base::span<const PickerCategory> kAllCategories = {(PickerCategory[]){
    PickerCategory::kEditorWrite,
    PickerCategory::kEditorRewrite,
    PickerCategory::kLinks,
    PickerCategory::kExpressions,
    PickerCategory::kClipboard,
    PickerCategory::kDriveFiles,
    PickerCategory::kLocalFiles,
    PickerCategory::kDatesTimes,
    PickerCategory::kUnitsMaths,
}};

using MockSearchResultsCallback =
    ::testing::MockFunction<PickerSearchRequest::SearchResultsCallback>;

class PickerSearchRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockSearchPickerClient& client() { return client_; }

  emoji::EmojiSearch& emoji_search() { return emoji_search_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockSearchPickerClient> client_;
  emoji::EmojiSearch emoji_search_;
};

TEST_F(PickerSearchRequestTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(Eq(u"cat"), _, _)).Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromOmniboxSearch) {
  MockSearchResultsCallback search_results_callback;
  // Catch-all to prevent unexpected gMock call errors. See
  // https://google.github.io/googletest/gmock_cook_book.html#uninteresting-vs-unexpected
  // for more details.
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(Property(
               "data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::BrowsingHistoryData>(
                   Field("url", &PickerSearchResult::BrowsingHistoryData::url,
                         Property("spec", &GURL::spec,
                                  "https://www.google.com/search?q=cat"))))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
}

TEST_F(PickerSearchRequestTest, TruncatesOmniboxResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3")))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::Text(u"1"), ash::PickerSearchResult::Text(u"2"),
       ash::PickerSearchResult::Text(u"3"),
       ash::PickerSearchResult::Text(u"4")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateOmniboxOnlyResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kOmnibox,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"4")))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", PickerCategory::kLinks,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::Text(u"1"), ash::PickerSearchResult::Text(u"2"),
       ash::PickerSearchResult::Text(u"3"),
       ash::PickerSearchResult::Text(u"4")});
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
      &client(), &emoji_search(), kAllCategories);
  after_start_search.Call();
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
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
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
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
        &client(), &emoji_search(), kAllCategories);
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
        &client(), &emoji_search(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kFileSearch,
        {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});
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
        &client(), &emoji_search(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerSearchResult::BrowsingHistory(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 1);
}

// TODO: b/333302795 - Add tests for searching emoji once EmojiSearch can be
// easily faked.

TEST_F(PickerSearchRequestTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kLocalFile,
                   ElementsAre(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(Field(
                           "text", &PickerSearchResult::TextData::primary_text,
                           u"monorail_cat.jpg")))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});
}

TEST_F(PickerSearchRequestTest, TruncatesResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3.jpg")))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"1.jpg"),
       ash::PickerSearchResult::Text(u"2.jpg"),
       ash::PickerSearchResult::Text(u"3.jpg"),
       ash::PickerSearchResult::Text(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateResultsFromFileOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"4.jpg")))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", PickerCategory::kLocalFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"1.jpg"),
       ash::PickerSearchResult::Text(u"2.jpg"),
       ash::PickerSearchResult::Text(u"3.jpg"),
       ash::PickerSearchResult::Text(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, RecordsFileMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});

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
        &client(), &emoji_search(), kAllCategories);
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
        &client(), &emoji_search(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerSearchResult::BrowsingHistory(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive,
                   ElementsAre(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(Field(
                           "text", &PickerSearchResult::TextData::primary_text,
                           u"catrbug_135117.jpg")))),
                   /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"catrbug_135117.jpg")});
}

TEST_F(PickerSearchRequestTest, TruncatesResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kDrive,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3.jpg")))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"1.jpg"),
       ash::PickerSearchResult::Text(u"2.jpg"),
       ash::PickerSearchResult::Text(u"3.jpg"),
       ash::PickerSearchResult::Text(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, DoesNotTruncateResultsFromDriveOnlySearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kDrive,
           ElementsAre(
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"1.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"2.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"3.jpg"))),
               Property("data", &PickerSearchResult::data,
                        VariantWith<PickerSearchResult::TextData>(Field(
                            "text", &PickerSearchResult::TextData::primary_text,
                            u"4.jpg")))),
           /*has_more_results=*/false))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", /*category=*/PickerCategory::kDriveFiles,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"1.jpg"),
       ash::PickerSearchResult::Text(u"2.jpg"),
       ash::PickerSearchResult::Text(u"3.jpg"),
       ash::PickerSearchResult::Text(u"4.jpg")});
}

TEST_F(PickerSearchRequestTest, SendsEmptyDriveResultsOnTimeout) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive, IsEmpty(),
                   /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(PickerSearchRequest::kDriveSearchTimeout);
}

TEST_F(PickerSearchRequestTest, IgnoresDriveResultsAfterTimeout) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive, IsEmpty(),
                   /*has_more_results=*/false))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive, Not(IsEmpty()), _))
      .Times(0);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(PickerSearchRequest::kDriveSearchTimeout);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"1.jpg")});
}

TEST_F(PickerSearchRequestTest, CancelsTimeoutTimerOnReceivingDriveResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive,
                   ElementsAre(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(Field(
                           "text", &PickerSearchResult::TextData::primary_text,
                           u"1.jpg")))),
                   /*has_more_results=*/false))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kDrive, IsEmpty(), _))
      .Times(0);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"1.jpg")});
  task_environment().FastForwardBy(PickerSearchRequest::kDriveSearchTimeout);
}

TEST_F(PickerSearchRequestTest, RecordsDriveMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"catrbug_135117.jpg")});

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
        &client(), &emoji_search(), kAllCategories);
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
        &client(), &emoji_search(), kAllCategories);
    client().cros_search_callback().Run(
        ash::AppListSearchResultType::kOmnibox,
        {ash::PickerSearchResult::BrowsingHistory(
            GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
            ui::ImageModel())});
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(PickerSearchRequestTest, DoesNotSendQueryToGifSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), FetchGifSearch(Eq("cat"), _)).Times(0);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
}

TEST_F(PickerSearchRequestTest, SendsQueryToGifSearchAfterDelay) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), FetchGifSearch(Eq("cat"), _)).Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);
}

TEST_F(PickerSearchRequestTest, ShowsResultsFromGifSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kTenor,
           Contains(Property(
               "data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::GifData>(AllOf(
                   Field("full_url", &PickerSearchResult::GifData::full_url,
                         Property("spec", &GURL::spec,
                                  "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                  "plink-cat-plink.gif")),
                   Field("content_description",
                         &PickerSearchResult::GifData::content_description,
                         u"cat blink"))))),
           /*has_more_results=*/true))
      .Times(AtLeast(1));

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);

  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

TEST_F(PickerSearchRequestTest, StopsOldGifSearches) {
  MockSearchResultsCallback search_results_callback;
  PickerClient::FetchGifsCallback old_gif_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kTenor,
           Contains(Property(
               "data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::GifData>(AllOf(
                   Field("full_url", &PickerSearchResult::GifData::full_url,
                         Property("spec", &GURL::spec,
                                  "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                  "plink-cat-plink.gif")),
                   Field("content_description",
                         &PickerSearchResult::GifData::content_description,
                         u"cat blink"))))),
           /*has_more_results=*/_))
      .Times(0);
  ON_CALL(client(), StopGifSearch)
      .WillByDefault(
          Invoke(&old_gif_callback, &PickerClient::FetchGifsCallback::Reset));

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        &client(), &emoji_search(), kAllCategories);
    task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);
    old_gif_callback = std::move(client().gif_search_callback());
    EXPECT_FALSE(old_gif_callback.is_null());
  }

  EXPECT_TRUE(old_gif_callback.is_null());
}

TEST_F(PickerSearchRequestTest, RecordsGifMetrics) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
  task_environment().FastForwardBy(kMetricMetricTime);
  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.GifProvider.QueryTime",
      kMetricMetricTime - PickerSearchRequest::kGifDebouncingDelay, 1);
}

TEST_F(PickerSearchRequestTest, DoesNotRecordGifMetricsIfNoResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  {
    PickerSearchRequest request(
        u"cat", std::nullopt,
        base::BindRepeating(&MockSearchResultsCallback::Call,
                            base::Unretained(&search_results_callback)),
        &client(), &emoji_search(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.GifProvider.QueryTime", 0);
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
      &client(), &emoji_search(), kAllCategories);
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
        &client(), &emoji_search(), kAllCategories);
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
      &client(), &emoji_search(), kAllCategories);
}

// TODO: crbug.com/40240570 - Re-enable once MSan stops failing on Rust-side
// allocations.
#if defined(MEMORY_SANITIZER)
#define MAYBE_PublishesMathResultsOnlyOnce DISABLED_PublishesMathResultsOnlyOnce
#else
#define MAYBE_PublishesMathResultsOnlyOnce PublishesMathResultsOnlyOnce
#endif
TEST_F(PickerSearchRequestTest, MAYBE_PublishesMathResultsOnlyOnce) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  PickerSearchRequest request(
      u"1 + 1", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
}

// TODO: crbug.com/40240570 - Re-enable once MSan stops failing on Rust-side
// allocations.
#if defined(MEMORY_SANITIZER)
#define MAYBE_RecordsMathMetricsOnlyOnce DISABLED_RecordsMathMetricsOnlyOnce
#else
#define MAYBE_RecordsMathMetricsOnlyOnce RecordsMathMetricsOnlyOnce
#endif
TEST_F(PickerSearchRequestTest, MAYBE_RecordsMathMetricsOnlyOnce) {
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
        &client(), &emoji_search(), kAllCategories);
  }

  histogram.ExpectTotalCount("Ash.Picker.Search.MathProvider.QueryTime", 1);
}

// TODO: crbug.com/40240570 - Re-enable once MSan stops failing on Rust-side
// allocations.
#if defined(MEMORY_SANITIZER)
#define MAYBE_PublishesMathResultsWhenMathCategorySelected \
  DISABLED_PublishesMathResultsWhenMathCategorySelected
#else
#define MAYBE_PublishesMathResultsWhenMathCategorySelected \
  PublishesMathResultsWhenMathCategorySelected
#endif
TEST_F(PickerSearchRequestTest,
       MAYBE_PublishesMathResultsWhenMathCategorySelected) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(PickerSearchSource::kMath, _, /*has_more_results=*/_))
      .Times(1);

  PickerSearchRequest request(
      u"1 + 1", PickerCategory::kUnitsMaths,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
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
  EXPECT_CALL(client(), FetchGifSearch(_, _)).Times(0);

  {
    PickerSearchRequest request(u"ant", PickerCategory::kLinks,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
  }
  {
    PickerSearchRequest request(u"bat", PickerCategory::kDriveFiles,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
  }
  {
    PickerSearchRequest request(u"cat", PickerCategory::kLocalFiles,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
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
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kClipboard,
           ElementsAre(Property(
               "data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::ClipboardData>(FieldsAre(
                   _, PickerSearchResult::ClipboardData::DisplayFormat::kText,
                   u"cat", std::nullopt)))),
           /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), kAllCategories);
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
      &client(), &emoji_search(), kAllCategories);

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
  EXPECT_CALL(
      search_results_callback,
      Call(source,
           ElementsAre(Property(
               "data", &PickerSearchResult::data,
               VariantWith<PickerSearchResult::EditorData>(Field(
                   "freeform_text",
                   &PickerSearchResult::EditorData::freeform_text,
                   Optional(Eq("quick brown fox jumped over lazy dog")))))),
           /*has_more_results=*/false))
      .Times(1);

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), {{category}});
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
      &client(), &emoji_search(), {});
}

TEST_P(PickerSearchRequestEditorTest, RecordsEditorMetrics) {
  const auto& [category, source] = GetParam();
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;

  PickerSearchRequest request(
      u"quick brown fox jumped over lazy dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)),
      &client(), &emoji_search(), {{category}});

  histogram.ExpectTotalCount("Ash.Picker.Search.EditorProvider.QueryTime", 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerSearchRequestEditorTest,
    testing::Values(std::make_pair(PickerCategory::kEditorWrite,
                                   PickerSearchSource::kEditorWrite),
                    std::make_pair(PickerCategory::kEditorRewrite,
                                   PickerSearchSource::kEditorRewrite)));

}  // namespace
}  // namespace ash
