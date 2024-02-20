// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
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
using testing::DoAll;
using testing::Each;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::IsEmpty;
using testing::IsSupersetOf;
using testing::NiceMock;
using testing::Not;
using testing::Property;
using testing::SaveArg;
using testing::VariantWith;

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(400);

// Matcher for the last element of a collection.
MATCHER_P(LastElement, matcher, "") {
  return !arg.empty() &&
         ExplainMatchResult(matcher, arg.back(), result_listener);
}

class MockPickerClient : public PickerClient {
 public:
  MockPickerClient() {
    // Set default behaviours. These can be overridden with `WillOnce` and
    // `WillRepeatedly`.
    ON_CALL(*this, StartCrosSearch)
        .WillByDefault(SaveArg<1>(cros_search_callback()));
    ON_CALL(*this, FetchGifSearch)
        .WillByDefault(
            Invoke(this, &MockPickerClient::FetchGifSearchToSetCallback));
  }

  std::unique_ptr<AshWebView> CreateWebView(
      const AshWebView::InitParams& params) override {
    ADD_FAILURE() << "CreateWebView should not be called in this unittest";
    return nullptr;
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
              (const std::u16string& query, CrosSearchResultsCallback callback),
              (override));
  MOCK_METHOD(void, StopCrosQuery, (), (override));

  // Set by the default `StartCrosSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `StartCrosSearch` callback.
  CrosSearchResultsCallback* cros_search_callback() {
    return &cros_search_callback_;
  }

  // Set by the default `FetchGifSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `FetchGifSearch` callback.
  FetchGifsCallback* gif_search_callback() { return &gif_search_callback_; }

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
    testing::MockFunction<PickerViewDelegate::SearchResultsCallback>;

class PickerSearchControllerTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerSearchControllerTest, DoesNotPublishResultsWhileSearching) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, SendsQueryToCrosSearchImmediately) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client, StartCrosSearch(Eq(u"cat"), _)).Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, DoesNotPublishResultsDuringBurnIn) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client,
                                    /*burn_in_period=*/base::Milliseconds(100));
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromCrosSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  // Catch-all to prevent unexpected gMock call errors. See
  // https://google.github.io/googletest/gmock_cook_book.html#uninteresting-vs-unexpected
  // for more details.
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching links"),
              Property(
                  "results", &PickerSearchResults::Section::results,
                  ElementsAre(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<
                          PickerSearchResult::BrowsingHistoryData>(Field(
                          "url", &PickerSearchResult::BrowsingHistoryData::url,
                          Property(
                              "spec", &GURL::spec,
                              "https://www.google.com/search?q=cat")))))))))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, DoesNotFlashEmptyResultsFromCrosSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
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
  ON_CALL(client, StopCrosQuery).WillByDefault([&search_started, &client]() {
    if (search_started) {
      client.cros_search_callback()->Run(AppListSearchResultType::kOmnibox, {});
    }
    search_started = false;
  });
  ON_CALL(client, StartCrosSearch)
      .WillByDefault(DoAll(SaveArg<1>(client.cros_search_callback()),
                           [&search_started, &client]() {
                             client.StopCrosQuery();
                             search_started = true;
                           }));
  // Function only used for the below `EXPECT_CALL` to ensure that we don't call
  // the search callback with an empty callback after the initial state.
  testing::MockFunction<void()> after_start_search;
  testing::Expectation after_start_search_call =
      EXPECT_CALL(after_start_search, Call).Times(1);
  EXPECT_CALL(first_search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      first_search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(
              AllOf(Property("heading", &PickerSearchResults::Section::heading,
                             u"Matching links"),
                    Property("results", &PickerSearchResults::Section::results,
                             IsEmpty()))))))
      .Times(0)
      .After(after_start_search_call);
  EXPECT_CALL(second_search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      second_search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(
              AllOf(Property("heading", &PickerSearchResults::Section::heading,
                             u"Matching links"),
                    Property("results", &PickerSearchResults::Section::results,
                             IsEmpty()))))))
      // This may be changed to 1 if the initial state has an empty "Matching
      // links" section.
      .Times(0);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&first_search_results_callback)));
  after_start_search.Call();
  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&second_search_results_callback)));
}

TEST_F(PickerSearchControllerTest, DoesNotSendQueryToGifSearchImmediately) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client, FetchGifSearch(Eq("cat"), _)).Times(0);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, SendsQueryToGifSearchAfterDelay) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client, FetchGifSearch(Eq("cat"), _)).Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromGifSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Other expressions"),
              Property(
                  "results", &PickerSearchResults::Section::results,
                  Contains(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::GifData>(AllOf(
                          Field("url", &PickerSearchResult::GifData::url,
                                Property(
                                    "spec", &GURL::spec,
                                    "https://media.tenor.com/GOabrbLMl4AAAAAd/"
                                    "plink-cat-plink.gif")),
                          Field(
                              "content_description",
                              &PickerSearchResult::GifData::content_description,
                              u"cat blink")))))))))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);

  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchController::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, StopsOldGifSearches) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  PickerClient::FetchGifsCallback old_gif_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Other expressions"),
              Property(
                  "results", &PickerSearchResults::Section::results,
                  Contains(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::GifData>(AllOf(
                          Field("url", &PickerSearchResult::GifData::url,
                                Property(
                                    "spec", &GURL::spec,
                                    "https://media.tenor.com/GOabrbLMl4AAAAAd/"
                                    "plink-cat-plink.gif")),
                          Field(
                              "content_description",
                              &PickerSearchResult::GifData::content_description,
                              u"cat blink")))))))))))
      .Times(0);
  ON_CALL(client, StopGifSearch)
      .WillByDefault(
          Invoke(&old_gif_callback, &PickerClient::FetchGifsCallback::Reset));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);
  old_gif_callback = std::move(*client.gif_search_callback());
  EXPECT_FALSE(old_gif_callback.is_null());
  controller.StartSearch(
      u"dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  EXPECT_TRUE(old_gif_callback.is_null());
}

TEST_F(PickerSearchControllerTest, ShowGifResultsLast) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          LastElement(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Other expressions"),
              Property(
                  "results", &PickerSearchResults::Section::results,
                  Contains(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::GifData>(AllOf(
                          Field("url", &PickerSearchResult::GifData::url,
                                Property("spec", &GURL::spec,
                                         "https://media.tenor.com/"
                                         "GOabrbLMl4AAAAAd/"
                                         "plink-cat-plink.gif")),
                          Field(
                              "content_description",
                              &PickerSearchResult::GifData::content_description,
                              u"cat blink")))))))))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);

  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchController::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, CombinesSearchResults) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          IsSupersetOf({
              AllOf(Property("heading", &PickerSearchResults::Section::heading,
                             u"Other expressions"),
                    Property(
                        "results", &PickerSearchResults::Section::results,
                        Contains(Property(
                            "data", &PickerSearchResult::data,
                            VariantWith<PickerSearchResult::GifData>(AllOf(
                                Field("url", &PickerSearchResult::GifData::url,
                                      Property("spec", &GURL::spec,
                                               "https://media.tenor.com/"
                                               "GOabrbLMl4AAAAAd/"
                                               "plink-cat-plink.gif")),
                                Field("content_description",
                                      &PickerSearchResult::GifData::
                                          content_description,
                                      u"cat blink"))))))),
              AllOf(
                  Property("heading", &PickerSearchResults::Section::heading,
                           u"Matching links"),
                  Property(
                      "results", &PickerSearchResults::Section::results,
                      ElementsAre(Property(
                          "data", &PickerSearchResult::data,
                          VariantWith<
                              PickerSearchResult::BrowsingHistoryData>(Field(
                              "url",
                              &PickerSearchResult::BrowsingHistoryData::url,
                              Property(
                                  "spec", &GURL::spec,
                                  "https://www.google.com/search?q=cat"))))))),

          }))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);

  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod -
                                   PickerSearchController::kGifDebouncingDelay);
}

TEST_F(PickerSearchControllerTest, DoNotShowEmptySections) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property("sections", &PickerSearchResults::sections,
                    Not(Contains(Property(
                        "heading", &PickerSearchResults::Section::heading,
                        u"Matching links"))))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(PickerSearchController::kGifDebouncingDelay);

  client.cros_search_callback()->Run(ash::AppListSearchResultType::kOmnibox,
                                     {});
  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchControllerTest, ShowGifResultsEvenAfterBurnIn) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client, kBurnInPeriod);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Other expressions"),
              Property(
                  "results", &PickerSearchResults::Section::results,
                  Contains(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::GifData>(AllOf(
                          Field("url", &PickerSearchResult::GifData::url,
                                Property("spec", &GURL::spec,
                                         "https://media.tenor.com/"
                                         "GOabrbLMl4AAAAAd/"
                                         "plink-cat-plink.gif")),
                          Field(
                              "content_description",
                              &PickerSearchResult::GifData::content_description,
                              u"cat blink")))))))))))
      .Times(AtLeast(1));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(480, 480), u"cat blink")});
}

}  // namespace
}  // namespace ash
