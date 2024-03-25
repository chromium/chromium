// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_request.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/emoji/emoji_search.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::IsEmpty;
using testing::IsSupersetOf;
using testing::NiceMock;
using testing::Property;
using testing::SaveArg;
using testing::VariantWith;

constexpr base::TimeDelta kMetricMetricTime = base::Milliseconds(300);

constexpr base::span<const PickerCategory> kAllCategories = {(PickerCategory[]){
    PickerCategory::kEmojis,
    PickerCategory::kSymbols,
    PickerCategory::kEmoticons,
    PickerCategory::kGifs,
    PickerCategory::kOpenTabs,
    PickerCategory::kBrowsingHistory,
    PickerCategory::kBookmarks,
}};

// TODO: b/329756078 - Deduplicate this with the `MockPickerClient` in
// `picker_search_controller_unittest`.
class MockPickerClient : public PickerClient {
 public:
  MockPickerClient() {
    // Set default behaviours. These can be overridden with `WillOnce` and
    // `WillRepeatedly`.
    ON_CALL(*this, StartCrosSearch)
        .WillByDefault(SaveArg<2>(&cros_search_callback_));
    ON_CALL(*this, FetchGifSearch)
        .WillByDefault(
            Invoke(this, &MockPickerClient::FetchGifSearchToSetCallback));
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override {
    ADD_FAILURE()
        << "GetSharedURLLoaderFactory should not be called in this unittest";
    return nullptr;
  }

  MOCK_METHOD(void,
              FetchGifSearch,
              (const std::string& query, FetchGifsCallback callback),
              (override));
  MOCK_METHOD(void, StopGifSearch, (), (override));
  MOCK_METHOD(void,
              StartCrosSearch,
              (const std::u16string& query,
               std::optional<PickerCategory> category,
               CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));
  MOCK_METHOD(void, ShowEditor, (), (override));

  // Set by the default `StartCrosSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `StartCrosSearch` callback.
  CrosSearchResultsCallback& cros_search_callback() {
    return cros_search_callback_;
  }

  // Set by the default `FetchGifSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `FetchGifSearch` callback.
  FetchGifsCallback& gif_search_callback() { return gif_search_callback_; }

  // Use `Invoke(&client, &MockPickerClient::FetchGifSearchToSetCallback)` as a
  // `FetchGifSearch` action to set `gif_search_callback_` when `FetchGifSearch`
  // is called.
  // This is already done by default in the constructor, but provided here for
  // convenience if a test requires this action when overriding the default
  // action.
  // TODO: b/73967242 - Use a gMock action for this once gMock supports
  // move-only arguments in `SaveArg`.
  void FetchGifSearchToSetCallback(const std::string& query,
                                   FetchGifsCallback callback) {
    gif_search_callback_ = std::move(callback);
  }

 private:
  CrosSearchResultsCallback cros_search_callback_;
  FetchGifsCallback gif_search_callback_;
};

using MockSearchResultsCallback =
    testing::MockFunction<PickerSearchRequest::SearchResultsCallback>;

class PickerSearchRequestTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockPickerClient& client() { return client_; }

  emoji::EmojiSearch& emoji_search() { return emoji_search_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockPickerClient> client_;
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
                                  "https://www.google.com/search?q=cat")))))))
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
  EXPECT_CALL(first_search_results_callback,
              Call(PickerSearchSource::kOmnibox, IsEmpty()))
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

TEST_F(PickerSearchRequestTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kLocalFile,
           ElementsAre(Property("data", &PickerSearchResult::data,
                                VariantWith<PickerSearchResult::TextData>(Field(
                                    "text", &PickerSearchResult::TextData::text,
                                    u"monorail_cat.jpg"))))))
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
  EXPECT_CALL(
      search_results_callback,
      Call(PickerSearchSource::kDrive,
           ElementsAre(Property("data", &PickerSearchResult::data,
                                VariantWith<PickerSearchResult::TextData>(Field(
                                    "text", &PickerSearchResult::TextData::text,
                                    u"catrbug_135117.jpg"))))))
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

TEST_F(PickerSearchRequestTest, DoesNotRecordDriveMetricsIfNoFileResponse) {
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
                         u"cat blink")))))))
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
                         u"cat blink")))))))
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

TEST_F(PickerSearchRequestTest, OnlyStartCrosSearchForCertainCategories) {
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"ant"), Eq(PickerCategory::kBookmarks), _))
      .Times(1);
  EXPECT_CALL(
      client(),
      StartCrosSearch(Eq(u"bat"), Eq(PickerCategory::kBrowsingHistory), _))
      .Times(1);
  EXPECT_CALL(client(),
              StartCrosSearch(Eq(u"cat"), Eq(PickerCategory::kOpenTabs), _))
      .Times(1);
  EXPECT_CALL(client(), FetchGifSearch(_, _)).Times(0);

  {
    PickerSearchRequest request(u"ant", PickerCategory::kBookmarks,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
  }
  {
    PickerSearchRequest request(u"bat", PickerCategory::kBrowsingHistory,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
  }
  {
    PickerSearchRequest request(u"cat", PickerCategory::kOpenTabs,
                                base::DoNothing(), &client(), &emoji_search(),
                                kAllCategories);
  }
}

}  // namespace
}  // namespace ash
