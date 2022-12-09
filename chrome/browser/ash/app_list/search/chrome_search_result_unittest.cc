// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using TextItem = ash::SearchResultTextItem;
using TextType = ash::SearchResultTextItemType;

TextItem CreateFakeStringTextItem(const std::u16string& text) {
  TextItem text_item(TextType::kString);
  text_item.SetText(text);
  text_item.SetTextTags({});

  return text_item;
}

class FakeChromeSearchResult : public ChromeSearchResult {
 public:
  FakeChromeSearchResult() = default;
  FakeChromeSearchResult(const FakeChromeSearchResult&) = delete;
  FakeChromeSearchResult& operator=(const FakeChromeSearchResult&) = delete;
  ~FakeChromeSearchResult() override = default;

 private:
  void Open(int event_flag) override {}
};

}  // namespace

class ChromeSearchResultTest : public testing::Test {
 public:
  ChromeSearchResultTest() = default;
  ~ChromeSearchResultTest() override = default;

 protected:
  void SetUp() override {
    result_ = std::make_unique<FakeChromeSearchResult>();
  }

  std::unique_ptr<ChromeSearchResult> result_;
};

TEST_F(ChromeSearchResultTest, TitleVector) {
  const std::u16string title1 = u"fake title1";
  result_->SetTitle(title1);

  std::vector<TextItem> title_vector = result_->title_text_vector();
  EXPECT_EQ(title_vector.size(), 1u);
  EXPECT_EQ(title_vector[0].GetText(), title1);

  // title_vector can be updated through |SetTitle| if not explicitly set by
  // chrome.
  const std::u16string title2 = u"fake title2";
  result_->SetTitle(title2);

  title_vector = result_->title_text_vector();
  EXPECT_EQ(title_vector.size(), 1u);
  EXPECT_EQ(title_vector[0].GetText(), title2);

  // Explicitly set title_vector.
  const std::u16string title3 = u"fake title3";
  std::vector<TextItem> title_vector_input1;
  title_vector_input1.emplace_back(CreateFakeStringTextItem(title3));
  result_->SetTitleTextVector(title_vector_input1);

  title_vector = result_->title_text_vector();
  EXPECT_EQ(title_vector.size(), 1u);
  EXPECT_EQ(title_vector[0].GetText(), title3);

  // title_vector cannot be updated through |SetTitle| if explicitly set by
  // chrome.
  const std::u16string title4 = u"fake title4";
  result_->SetTitle(title4);

  title_vector = result_->title_text_vector();
  EXPECT_EQ(title_vector.size(), 1u);
  EXPECT_EQ(title_vector[0].GetText(), title3);

  // title_vector can still be updated through |SetTitleTextVector| if
  // explicitly set by chrome.
  const std::u16string title5 = u"fake title5";
  std::vector<TextItem> title_vector_input2;
  title_vector_input2.emplace_back(CreateFakeStringTextItem(title5));
  result_->SetTitleTextVector(title_vector_input2);

  title_vector = result_->title_text_vector();
  EXPECT_EQ(title_vector.size(), 1u);
  EXPECT_EQ(title_vector[0].GetText(), title5);
}

TEST_F(ChromeSearchResultTest, DetailsVector) {
  const std::u16string details1 = u"fake details1";
  result_->SetDetails(details1);

  std::vector<TextItem> details_vector = result_->details_text_vector();
  EXPECT_EQ(details_vector.size(), 1u);
  EXPECT_EQ(details_vector[0].GetText(), details1);

  // details_vector can be updated through |SetDetails| if not explicitly set by
  // chrome.
  const std::u16string details2 = u"fake details2";
  result_->SetDetails(details2);

  details_vector = result_->details_text_vector();
  EXPECT_EQ(details_vector.size(), 1u);
  EXPECT_EQ(details_vector[0].GetText(), details2);

  // Explicitly set details_vector.
  const std::u16string details3 = u"fake details3";
  std::vector<TextItem> details_vector_input1;
  details_vector_input1.emplace_back(CreateFakeStringTextItem(details3));
  result_->SetDetailsTextVector(details_vector_input1);

  details_vector = result_->details_text_vector();
  EXPECT_EQ(details_vector.size(), 1u);
  EXPECT_EQ(details_vector[0].GetText(), details3);

  // details_vector cannot be updated through |SetDetails| if explicitly set by
  // chrome.
  const std::u16string details4 = u"fake details4";
  result_->SetDetails(details4);

  details_vector = result_->details_text_vector();
  EXPECT_EQ(details_vector.size(), 1u);
  EXPECT_EQ(details_vector[0].GetText(), details3);

  // details_vector can still be updated through |SetDetailsTextVector| if
  // explicitly set by chrome.
  const std::u16string details5 = u"fake details5";
  std::vector<TextItem> details_vector_input2;
  details_vector_input2.emplace_back(CreateFakeStringTextItem(details5));
  result_->SetDetailsTextVector(details_vector_input2);

  details_vector = result_->details_text_vector();
  EXPECT_EQ(details_vector.size(), 1u);
  EXPECT_EQ(details_vector[0].GetText(), details5);
}

}  // namespace app_list::test
