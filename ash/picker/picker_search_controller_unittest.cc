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

  void DownloadGifToString(const ValidGifUrl& query,
                           DownloadGifToStringCallback callback) override {
    FAIL() << "DownloadGifToString should not be called in this unittest";
  }

  MOCK_METHOD(void,
              FetchGifSearch,
              (const std::string& query, FetchGifsCallback callback),
              (override));
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

using PickerSearchControllerTest = testing::Test;

TEST_F(PickerSearchControllerTest, ShowsInitialHeadingsOnSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          ElementsAre(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching expressions"),
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching links")))))
      .Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, SendsQueryToCrosSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client, StartCrosSearch(Eq(u"cat"), _)).Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromCrosSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
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
}

TEST_F(PickerSearchControllerTest, DoesNotFlashEmptyResultsFromCrosSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  PickerSearchResults last_results;
  // CrOS search calls `StopSearch()` automatically on starting a search.
  // If `StopSearch` actually stops a search, some providers such as the omnibox
  // automatically call the search result callback from the _last_ search with
  // an empty vector.
  // Ensure that we don't flash empty results if this happens - i.e. that we
  // call `StopSearch` before starting a new search.
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
  ON_CALL(search_results_callback, Call)
      .WillByDefault(SaveArg<0>(&last_results));

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  controller.StartSearch(
      u"dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  EXPECT_THAT(
      last_results,
      Property("sections", &PickerSearchResults::sections,
               Each(Property("results", &PickerSearchResults::Section::results,
                             Not(IsEmpty())))));
}

TEST_F(PickerSearchControllerTest, SendsQueryToGifSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  NiceMock<MockSearchResultsCallback> search_results_callback;
  EXPECT_CALL(client, FetchGifSearch(Eq("cat"), _)).Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, ShowsResultsFromGifSearch) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching expressions"),
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

  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

TEST_F(PickerSearchControllerTest, DoesNotShowOldGifSearchResults) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          Contains(AllOf(
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching expressions"),
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

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  PickerClient::FetchGifsCallback first_callback =
      std::move(*client.gif_search_callback());
  controller.StartSearch(
      u"dog", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  std::move(first_callback)
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

TEST_F(PickerSearchControllerTest, CombinesSearchResults) {
  NiceMock<MockPickerClient> client;
  PickerSearchController controller(&client);
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Property(
          "sections", &PickerSearchResults::sections,
          IsSupersetOf({
              AllOf(Property("heading", &PickerSearchResults::Section::heading,
                             u"Matching expressions"),
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

  client.cros_search_callback()->Run(
      ash::AppListSearchResultType::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  std::move(*client.gif_search_callback())
      .Run({ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

}  // namespace
}  // namespace ash
