// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/mock_search_picker_client.h"
#include "ash/picker/search/picker_search_request.h"
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
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(400);

// Before burn-in, after GIF debouncing.
constexpr base::TimeDelta kBeforeBurnIn = base::Milliseconds(300);
static_assert(PickerSearchRequest::kGifDebouncingDelay < kBeforeBurnIn);
static_assert(kBeforeBurnIn < kBurnInPeriod);

constexpr base::TimeDelta kAfterBurnIn = base::Milliseconds(700);
static_assert(kBurnInPeriod < kAfterBurnIn);

constexpr base::span<const PickerCategory> kAllCategories = {(PickerCategory[]){
    PickerCategory::kEditor,
    PickerCategory::kLinks,
    PickerCategory::kExpressions,
    PickerCategory::kClipboard,
    PickerCategory::kDriveFiles,
    PickerCategory::kLocalFiles,
    PickerCategory::kDatesTimes,
    PickerCategory::kUnitsMaths,
}};

// Matcher for the last element of a collection.
MATCHER_P(LastElement, matcher, "") {
  return !arg.empty() &&
         ExplainMatchResult(matcher, arg.back(), result_listener);
}

using MockSearchResultsCallback =
    ::testing::MockFunction<PickerViewDelegate::SearchResultsCallback>;

class PickerSearchControllerTest : public testing::Test {
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

TEST_F(PickerSearchControllerTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(Eq(u"cat"), _, _)).Times(1);
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);
  PickerSearchController controller(&client(),
                                    /*burn_in_period=*/base::Milliseconds(100));

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromOmniboxSearch) {
  MockSearchResultsCallback search_results_callback;
  // Catch-all to prevent unexpected gMock call errors. See
  // https://google.github.io/googletest/gmock_cook_book.html#uninteresting-vs-unexpected
  // for more details.
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::BrowsingHistoryData>(Field(
                      "url", &PickerSearchResult::BrowsingHistoryData::url,
                      Property("spec", &GURL::spec,
                               "https://www.google.com/search?q=cat"))))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, DoesNotFlashEmptyResultsFromOmniboxSearch) {
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
              Call(Contains(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLinks),
                  Property("results", &PickerSearchResultsSection::results,
                           IsEmpty())))))
      .Times(0)
      .After(after_start_search_call);
  EXPECT_CALL(second_search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(second_search_results_callback,
              Call(Contains(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLinks),
                  Property("results", &PickerSearchResultsSection::results,
                           IsEmpty())))))
      // This may be changed to 1 if the initial state has an empty links
      // section.
      .Times(0);
  // As `StopCrosQuery` may be called in the destructor of
  // `PickerSearchController`, ensure that it gets destructed before any of the
  // variables used in the above mocks are used.
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)));
  after_start_search.Call();
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&second_search_results_callback)));
}

TEST_F(PickerSearchControllerTest, RecordsOmniboxMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.OmniboxProvider.QueryTime", kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsOmniboxMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.OmniboxProvider.QueryTime", kAfterBurnIn, 1);
}

TEST_F(PickerSearchControllerTest,
       DoesNotRecordOmniboxMetricsIfNoOmniboxResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest,
       DoesNotRecordOmniboxMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(
    PickerSearchControllerTest,
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
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&second_search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 1);
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kFiles),
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(Field(
                           "text", &PickerSearchResult::TextData::primary_text,
                           u"monorail_cat.jpg")))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, RecordsFileMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.FileProvider.QueryTime",
                                   kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsFileMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerSearchResult::Text(u"monorail_cat.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.FileProvider.QueryTime",
                                   kAfterBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, DoesNotRecordFileMetricsIfNoFileResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest,
       DoesNotRecordFileMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kDriveFiles),
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(Field(
                           "text", &PickerSearchResult::TextData::primary_text,
                           u"catrbug_135117.jpg")))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"catrbug_135117.jpg")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, RecordsDriveMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"catrbug_135117.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.DriveProvider.QueryTime",
                                   kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsDriveMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"catrbug_135117.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.DriveProvider.QueryTime",
                                   kAfterBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, DoesNotRecordDriveMetricsIfNoDriveResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest,
       DoesNotRecordDriveMetricsIfOtherCrosSearchResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .Times(AtLeast(2))
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .Times(2)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest, DoesNotSendQueryToGifSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), FetchGifSearch(Eq("cat"), _)).Times(0);
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, SendsQueryToGifSearchAfterDelay) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), FetchGifSearch(Eq("cat"), _)).Times(1);
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest,
       ShowsResultsFromGifSearchAfterDriveSearchFinishes) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kGifs),
          Property(
              "results", &PickerSearchResultsSection::results,
              Contains(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::GifData>(AllOf(
                      Field("full_url", &PickerSearchResult::GifData::full_url,
                            Property("spec", &GURL::spec,
                                     "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                     "plink-cat-plink.gif")),
                      Field("content_description",
                            &PickerSearchResult::GifData::content_description,
                            u"cat blink"))))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"drive")});
  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchRequest::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, StopsOldGifSearches) {
  MockSearchResultsCallback search_results_callback;
  PickerClient::FetchGifsCallback old_gif_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kGifs),
          Property(
              "results", &PickerSearchResultsSection::results,
              Contains(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::GifData>(AllOf(
                      Field("full_url", &PickerSearchResult::GifData::full_url,
                            Property("spec", &GURL::spec,
                                     "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                     "plink-cat-plink.gif")),
                      Field("content_description",
                            &PickerSearchResult::GifData::content_description,
                            u"cat blink"))))))))))
      .Times(0);
  ON_CALL(client(), StopGifSearch)
      .WillByDefault(
          Invoke(&old_gif_callback, &PickerClient::FetchGifsCallback::Reset));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);
  old_gif_callback = std::move(client().gif_search_callback());
  EXPECT_FALSE(old_gif_callback.is_null());
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  EXPECT_TRUE(old_gif_callback.is_null());
}

TEST_F(PickerSearchControllerTest, ShowGifResultsLast) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(LastElement(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kGifs),
                  Property("results", &PickerSearchResultsSection::results,
                           Contains(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"gif")))))))))
      .Times(1);
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);

  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Text(u"gif")});
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"drive")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchRequest::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, RecordsGifMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.GifProvider.QueryTime",
      kBeforeBurnIn - PickerSearchRequest::kGifDebouncingDelay, 1);
}

TEST_F(PickerSearchControllerTest, RecordsGifMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.GifProvider.QueryTime",
      kAfterBurnIn - PickerSearchRequest::kGifDebouncingDelay, 1);
}

TEST_F(PickerSearchControllerTest, DoesNotRecordGifMetricsIfNoResponse) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StartSearch(
      u"dog", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  histogram.ExpectTotalCount("Ash.Picker.Search.GifProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest, CombinesSearchResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(IsSupersetOf({
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"omnibox")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kFiles),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"file")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"drive")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kGifs),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"gif")))))),
      })))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::Text(u"omnibox")});
  client().cros_search_callback().Run(ash::AppListSearchResultType::kFileSearch,
                                      {ash::PickerSearchResult::Text(u"file")});
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerSearchResult::Text(u"drive")});
  std::move(client().gif_search_callback())
      .Run({ash::PickerSearchResult::Text(u"gif")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchRequest::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, DoNotShowEmptySectionsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Not(Contains(Property("type", &PickerSearchResultsSection::type,
                                 PickerSectionType::kLinks)))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchRequest::kGifDebouncingDelay);

  client().cros_search_callback().Run(ash::AppListSearchResultType::kOmnibox,
                                      {});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, DoNotShowEmptySectionsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Not(Contains(Property("type", &PickerSearchResultsSection::type,
                                 PickerSectionType::kLinks)))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  client().cros_search_callback().Run(ash::AppListSearchResultType::kOmnibox,
                                      {});
}

TEST_F(PickerSearchControllerTest, ShowResultsEvenAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property("results", &PickerSearchResultsSection::results,
                   Contains(Property(
                       "data", &PickerSearchResult::data,
                       VariantWith<PickerSearchResult::TextData>(AllOf(
                           Field("primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"test"))))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(
      u"cat", std::nullopt, kAllCategories,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
  std::move(client().cros_search_callback())
      .Run(ash::AppListSearchResultType::kOmnibox,
           {ash::PickerSearchResult::Text(u"test")});
}

TEST_F(PickerSearchControllerTest, OnlyStartCrosSearchForCertainCategories) {
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
  PickerSearchController controller(&client(), kBurnInPeriod);

  controller.StartSearch(u"ant", PickerCategory::kLinks, kAllCategories,
                         base::DoNothing());
  controller.StartSearch(u"bat", PickerCategory::kDriveFiles, kAllCategories,
                         base::DoNothing());
  controller.StartSearch(u"cat", PickerCategory::kLocalFiles, kAllCategories,
                         base::DoNothing());
}

}  // namespace
}  // namespace ash
