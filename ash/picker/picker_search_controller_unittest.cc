// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_search_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace ash {
namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::NiceMock;
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
              StartCrosSearch,
              (const std::u16string& query, CrosSearchResultsCallback callback),
              (override));

  // Set by the default `StartCrosSearch` behaviour. If the behaviour is
  // overridden, this may not be set on a `StartCrosSearch` callback.
  CrosSearchResultsCallback* cros_search_callback() {
    return &cros_search_callback_;
  }

 private:
  CrosSearchResultsCallback cros_search_callback_;
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
                       u"Matching links"),
              Property("heading", &PickerSearchResults::Section::heading,
                       u"Matching files")))))
      .Times(1);

  controller.StartSearch(
      u"cat", std::nullopt,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
}

TEST_F(PickerSearchControllerTest, SendsQueryToCrosSearch) {
  MockPickerClient client;
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

}  // namespace
}  // namespace ash
