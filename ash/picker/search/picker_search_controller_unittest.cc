// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/mock_search_picker_client.h"
#include "ash/picker/search/picker_search_request.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/components/emoji/grit/emoji.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/mock_resource_bundle_delegate.h"
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

constexpr base::TimeDelta kBeforeBurnIn = base::Milliseconds(300);
static_assert(kBeforeBurnIn < kBurnInPeriod);

constexpr base::TimeDelta kAfterBurnIn = base::Milliseconds(700);
static_assert(kBurnInPeriod < kAfterBurnIn);

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

constexpr PickerSearchRequest::Options kDefaultSearchOptions{
    .available_categories = kAllCategories,
    .caps_lock_state_to_search = false,
};

// Matcher for the last element of a collection.
MATCHER_P(LastElement, matcher, "") {
  return !arg.empty() &&
         ExplainMatchResult(matcher, arg.back(), result_listener);
}

using MockSearchResultsCallback =
    ::testing::MockFunction<PickerViewDelegate::SearchResultsCallback>;

using MockEmojiSearchResultsCallback =
    ::testing::MockFunction<PickerViewDelegate::EmojiSearchResultsCallback>;

class PickerSearchControllerTest : public testing::Test {
 protected:
  PickerSearchControllerTest() {
    ON_CALL(client(), GetPrefs).WillByDefault(testing::Return(&prefs_service_));
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockSearchPickerClient& client() { return client_; }

  TestingPrefServiceSimple& prefs_service() { return prefs_service_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  NiceMock<MockSearchPickerClient> client_;
  TestingPrefServiceSimple prefs_service_;
};

struct FakeResource {
  int resource;
  std::string data;
};

class ScopedFakeResourceBundleDelegate {
 public:
  explicit ScopedFakeResourceBundleDelegate(
      base::span<const FakeResource> resources) {
    original_resource_bundle_ =
        ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", &delegate_, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

    for (const auto& [resource, data] : resources) {
      ON_CALL(delegate_, LoadDataResourceString(resource))
          .WillByDefault(testing::Return(data));
    }
  }

  ~ScopedFakeResourceBundleDelegate() {
    ui::ResourceBundle::CleanupSharedInstance();
    ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle_);
  }

 private:
  testing::NiceMock<ui::MockResourceBundleDelegate> delegate_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

TEST_F(PickerSearchControllerTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client(), StartCrosSearch(Eq(u"cat"), _, _)).Times(1);
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);
  PickerSearchController controller(/*burn_in_period=*/base::Milliseconds(100));

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
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
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(VariantWith<PickerBrowsingHistoryResult>(Field(
                       "url", &PickerBrowsingHistoryResult::url,
                       Property("spec", &GURL::spec,
                                "https://www.google.com/search?q=cat")))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
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
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)));
  after_start_search.Call();
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      &client(), u"dog", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&second_search_results_callback)));
}

TEST_F(PickerSearchControllerTest, RecordsOmniboxMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});

  histogram.ExpectUniqueTimeSample(
      "Ash.Picker.Search.OmniboxProvider.QueryTime", kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsOmniboxMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StopSearch();

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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});
  controller.StopSearch();

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 0);
}

TEST_F(
    PickerSearchControllerTest,
    DoesNotRecordOmniboxMetricsTwiceIfSearchResultsArePublishedAfterStopSearch) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  // CrOS search calls `StopSearch()` automatically on starting a search.
  // If `StopSearch` actually stops a search, some providers such as the omnibox
  // automatically call the search result callback from the _last_ search with
  // an empty vector.
  // Ensure that we don't record metrics twice if this happens.
  bool search_started = false;
  EXPECT_CALL(client(), StopCrosQuery)
      .WillRepeatedly([&search_started, this]() {
        if (search_started) {
          client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                              {});
        }
        search_started = false;
      });
  EXPECT_CALL(client(), StartCrosSearch)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StopSearch();

  histogram.ExpectTotalCount("Ash.Picker.Search.OmniboxProvider.QueryTime", 1);
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromFileSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLocalFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(
                               Field("text", &PickerTextResult::primary_text,
                                     u"monorail_cat.jpg"))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, RecordsFileMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.FileProvider.QueryTime",
                                   kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsFileMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kFileSearch,
      {ash::PickerTextResult(u"monorail_cat.jpg")});

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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StopSearch();

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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StopSearch();

  histogram.ExpectTotalCount("Ash.Picker.Search.FileProvider.QueryTime", 0);
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromDriveSearch) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kDriveFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(
                               Field("text", &PickerTextResult::primary_text,
                                     u"catrbug_135117.jpg"))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"catrbug_135117.jpg")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, RecordsDriveMetricsBeforeBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"catrbug_135117.jpg")});

  histogram.ExpectUniqueTimeSample("Ash.Picker.Search.DriveProvider.QueryTime",
                                   kBeforeBurnIn, 1);
}

TEST_F(PickerSearchControllerTest, RecordsDriveMetricsAfterBurnIn) {
  base::HistogramTester histogram;
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kAfterBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"catrbug_135117.jpg")});

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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  controller.StopSearch();

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
      .Times(1)
      .WillRepeatedly([&search_started, this](
                          const std::u16string& query,
                          std::optional<PickerCategory> category,
                          PickerClient::CrosSearchResultsCallback callback) {
        client().StopCrosQuery();
        search_started = true;
        client().cros_search_callback() = std::move(callback);
      });
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerBrowsingHistoryResult(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StopSearch();

  histogram.ExpectTotalCount("Ash.Picker.Search.DriveProvider.QueryTime", 0);
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
                         Contains(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"omnibox"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLocalFiles),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"file"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         Contains(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"drive"))))),
      })))
      .Times(AtLeast(1));
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);

  client().cros_search_callback().Run(ash::AppListSearchResultType::kOmnibox,
                                      {ash::PickerTextResult(u"omnibox")});
  client().cros_search_callback().Run(ash::AppListSearchResultType::kFileSearch,
                                      {ash::PickerTextResult(u"file")});
  client().cros_search_callback().Run(
      ash::AppListSearchResultType::kDriveSearch,
      {ash::PickerTextResult(u"drive")});
  task_environment().FastForwardBy(kBurnInPeriod - kBeforeBurnIn);
}

TEST_F(PickerSearchControllerTest, DoNotShowEmptySectionsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"zz", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBeforeBurnIn);

  client().cros_search_callback().Run(ash::AppListSearchResultType::kOmnibox,
                                      {});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, DoNotShowEmptySectionsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"zz", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  client().cros_search_callback().Run(ash::AppListSearchResultType::kOmnibox,
                                      {});
}

TEST_F(PickerSearchControllerTest, ShowResultsEvenAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLinks),
                  Property("results", &PickerSearchResultsSection::results,
                           Contains(VariantWith<PickerTextResult>(AllOf(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"test")))))))))
      .Times(AtLeast(1));
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
  std::move(client().cros_search_callback())
      .Run(ash::AppListSearchResultType::kOmnibox,
           {ash::PickerTextResult(u"test")});
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
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(&client(), u"ant", PickerCategory::kLinks,
                         kDefaultSearchOptions, base::DoNothing());
  controller.StartSearch(&client(), u"bat", PickerCategory::kDriveFiles,
                         kDefaultSearchOptions, base::DoNothing());
  controller.StartSearch(&client(), u"cat", PickerCategory::kLocalFiles,
                         kDefaultSearchOptions, base::DoNothing());
}

TEST_F(PickerSearchControllerTest,
       PublishesEmptyResultsAfterResultsOnceDoneDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    // "cat" shouldn't match any categories.
    EXPECT_CALL(
        search_results_callback,
        Call(Contains(Property("type", &PickerSearchResultsSection::type,
                               PickerSectionType::kLinks))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt,
      {.available_categories = {{PickerCategory::kLinks}}},
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
}

TEST_F(PickerSearchControllerTest,
       PublishesEmptyResultsAfterResultsOnceDoneAfterDoneAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    // "cat" shouldn't match any categories.
    EXPECT_CALL(
        search_results_callback,
        Call(Contains(Property("type", &PickerSearchResultsSection::type,
                               PickerSectionType::kLinks))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt,
      {.available_categories = {{PickerCategory::kLinks}}},
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
}

TEST_F(PickerSearchControllerTest,
       DoesNotPublishResultsWhenInterruptedDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(Property("type", &PickerSearchResultsSection::type,
                                     PickerSectionType::kLinks))))
      .Times(0);
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
  controller.StopSearch();
}

TEST_F(PickerSearchControllerTest,
       DoesNotPublishEmptyResultsWhenInterruptedDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBeforeBurnIn);
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
  controller.StopSearch();
}

TEST_F(PickerSearchControllerTest,
       DoesNotPublishEmptyResultsWhenInterruptedAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);
  NiceMock<MockSearchResultsCallback> second_search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
  controller.StopSearch();
}

TEST_F(PickerSearchControllerTest, StopSearchDoesNotCallOldCallbackAfterwards) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);
  MockSearchResultsCallback second_search_results_callback;
  PickerSearchController controller(kBurnInPeriod);

  controller.StartSearch(
      &client(), u"cat", std::nullopt, kDefaultSearchOptions,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client().cros_search_callback().Run(AppListSearchResultType::kOmnibox,
                                      {PickerTextResult(u"cat")});
  controller.StopSearch();
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, LoadsEmojiDataInAllLanguages) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀en","name":"grinning face",
            "keywords":["face","grin","grinning face",":D","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                     R"([])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀jp","name":"grinning face",
            "keywords":["笑顔","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING, R"([])"}}});
  prefs_service().registry()->RegisterStringPref(
      language::prefs::kApplicationLocale, "");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknknacl_mozc_jp");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng,"
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:jp::jpn,"
      "_comp_ime_jkghodnilhceideoidjikpgommlajknknacl_mozc_jp,"
      "_comp_ime_jkghodnilhceideoidjikpgommlajknknacl_mozc_us");
  prefs_service().registry()->RegisterDictionaryPref(
      prefs::kEmojiPickerPreferences, base::Value::Dict());
  MockEmojiSearchResultsCallback results_callback;
  EXPECT_CALL(
      results_callback,
      Call(ElementsAre(
          // JP is first because the current input method is a JP input method
          Field("text", &PickerEmojiResult::text, Eq(u"😀jp")),
          // The rest is from English
          Field("text", &PickerEmojiResult::text, Eq(u"😀en")),
          Field("text", &PickerEmojiResult::text, Eq(u":-)")))))
      .Times(1);

  PickerSearchController controller(
      /*burn_in_period=*/base::Milliseconds(100));
  controller.LoadEmojiLanguagesFromPrefs(&prefs_service());
  controller.StartEmojiSearch(
      &prefs_service(), u"smile",
      base::BindRepeating(&MockEmojiSearchResultsCallback::Call,
                          base::Unretained(&results_callback)));
}

TEST_F(PickerSearchControllerTest,
       LoadsEmojiDataInDefaultEnglishIfNoSupportedLanguage) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀en","name":"grinning face",
            "keywords":["face","grin","grinning face",":D","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                     R"([])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"}}});
  prefs_service().registry()->RegisterStringPref(
      language::prefs::kApplicationLocale, "en-US");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:notareallanguage");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:notareallanguage"
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:someotherfakelanguage");
  prefs_service().registry()->RegisterDictionaryPref(
      prefs::kEmojiPickerPreferences, base::Value::Dict());
  MockEmojiSearchResultsCallback results_callback;
  EXPECT_CALL(
      results_callback,
      Call(ElementsAre(Field("text", &PickerEmojiResult::text, Eq(u"😀en")),
                       Field("text", &PickerEmojiResult::text, Eq(u":-)")))))
      .Times(1);

  PickerSearchController controller(
      /*burn_in_period=*/base::Milliseconds(100));
  controller.LoadEmojiLanguagesFromPrefs(&prefs_service());
  controller.StartEmojiSearch(
      &prefs_service(), u"smile",
      base::BindRepeating(&MockEmojiSearchResultsCallback::Call,
                          base::Unretained(&results_callback)));
}

TEST_F(PickerSearchControllerTest, LoadsEmojiDataOnPrefsChange) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀en","name":"grinning face",
            "keywords":["face","grin","grinning face",":D","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                     R"([])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀jp","name":"grinning face",
            "keywords":["笑顔","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING, R"([])"}}});
  prefs_service().registry()->RegisterStringPref(
      language::prefs::kApplicationLocale, "");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng");
  prefs_service().registry()->RegisterDictionaryPref(
      prefs::kEmojiPickerPreferences, base::Value::Dict());

  PickerSearchController controller(
      /*burn_in_period=*/base::Milliseconds(100));

  // First search, only English results
  controller.LoadEmojiLanguagesFromPrefs(&prefs_service());
  MockEmojiSearchResultsCallback results_callback;
  EXPECT_CALL(results_callback,
              Call(ElementsAre(
                  // Only English Results
                  Field("text", &PickerEmojiResult::text, Eq(u"😀en")),
                  Field("text", &PickerEmojiResult::text, Eq(u":-)")))))
      .Times(1);
  controller.StartEmojiSearch(
      &prefs_service(), u"smile",
      base::BindRepeating(&MockEmojiSearchResultsCallback::Call,
                          base::Unretained(&results_callback)));

  // Second search after adding a Japanese IME should include Japanese results
  prefs_service().SetUserPref(
      prefs::kLanguagePreloadEngines,
      base::Value("_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng,"
                  "_comp_ime_jkghodnilhceideoidjikpgommlajknknacl_mozc_jp,"));
  MockEmojiSearchResultsCallback results_callback_jp;
  EXPECT_CALL(
      results_callback_jp,
      Call(ElementsAre(Field("text", &PickerEmojiResult::text, Eq(u"😀en")),
                       Field("text", &PickerEmojiResult::text, Eq(u"😀jp")),
                       Field("text", &PickerEmojiResult::text, Eq(u":-)")))))
      .Times(1);
  controller.StartEmojiSearch(
      &prefs_service(), u"smile",
      base::BindRepeating(&MockEmojiSearchResultsCallback::Call,
                          base::Unretained(&results_callback_jp)));
}

TEST_F(PickerSearchControllerTest, LoadsEmojiDataForJapaneseUiLocale) {
  ScopedFakeResourceBundleDelegate mock_resource_delegate(
      {{FakeResource{
            IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_START,
            R"([{"emoji":[{"base":{"string":"😀en","name":"grinning face",
            "keywords":["face","grin","grinning face",":D","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOJI_15_0_ORDERING_JSON_REMAINING,
                     R"([])"},
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_EN_INTERNAL, R"([])"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_ORDERING_JSON,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow"}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_SYMBOL_JA,
                     R"([{"group":"Arrows","emoji":[{"base":
            {"string":"←","name":"leftwards arrow","keywords":["矢印"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_EMOTICON_ORDERING_JSON,
                     R"-([{"group":"Classic","emoji":[
              {"base":{"string":":-)","name":"smiley face "}}]}])-"},
        FakeResource{
            IDR_EMOJI_PICKER_JA_START,
            R"([{"emoji":[{"base":{"string":"😀jp","name":"grinning face",
            "keywords":["笑顔","smile"]}}]}])"},
        FakeResource{IDR_EMOJI_PICKER_JA_REMAINING, R"([])"}}});
  prefs_service().registry()->RegisterStringPref(
      language::prefs::kApplicationLocale, "ja-JP");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng");
  prefs_service().registry()->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      "_comp_ime_jkghodnilhceideoidjikpgommlajknkxkb:us::eng");
  prefs_service().registry()->RegisterDictionaryPref(
      prefs::kEmojiPickerPreferences, base::Value::Dict());

  PickerSearchController controller(
      /*burn_in_period=*/base::Milliseconds(100));

  controller.LoadEmojiLanguagesFromPrefs(&prefs_service());
  MockEmojiSearchResultsCallback results_callback_jp;
  EXPECT_CALL(
      results_callback_jp,
      Call(ElementsAre(Field("text", &PickerEmojiResult::text, Eq(u"😀en")),
                       Field("text", &PickerEmojiResult::text, Eq(u"😀jp")),
                       Field("text", &PickerEmojiResult::text, Eq(u":-)")))))
      .Times(1);
  controller.StartEmojiSearch(
      &prefs_service(), u"smile",
      base::BindRepeating(&MockEmojiSearchResultsCallback::Call,
                          base::Unretained(&results_callback_jp)));
}

}  // namespace
}  // namespace ash
