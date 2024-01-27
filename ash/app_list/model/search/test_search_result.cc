// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/gfx/image/image_unittest_util.h"

#include "ash/app_list/model/search/test_search_result.h"

namespace ash {

namespace {

std::vector<SearchResult::TextItem> StringToTextVector(
    const std::u16string& text) {
  std::vector<SearchResult::TextItem> text_vector;
  SearchResult::TextItem text_item(ash::SearchResultTextItemType::kString);
  text_item.SetText(text);
  text_item.SetTextTags({});
  text_vector.push_back(text_item);
  return text_vector;
}

}  // namespace

TestSearchResult::TestSearchResult() = default;

TestSearchResult::~TestSearchResult() = default;

void TestSearchResult::set_result_id(const std::string& id) {
  set_id(id);
}

void TestSearchResult::SetTitle(const std::u16string& title) {
  SearchResult::SetTitle(title);
  SetTitleTextVector(StringToTextVector(title));
}

void TestSearchResult::SetDetails(const std::u16string& details) {
  SearchResult::SetDetails(details);
  SetDetailsTextVector(StringToTextVector(details));
}

void TestSearchResult::SetCategory(
    const ash::AppListSearchResultCategory category) {
  SearchResult::set_category(category);
}

void TestSearchResult::SetSystemInfoAnswerCardData(
    const ash::SystemInfoAnswerCardData& system_info_data) {
  SearchResult::set_system_info_answer_card_data(system_info_data);
}

void TestSearchResult::SetIconAndBadgeIcon() {
  const int icon_size = 28;
  const int badge_icon_size = 14;
  const SearchResultIconInfo test_icon_info(
      ui::ImageModel::FromImageSkia(
          gfx::test::CreateImageSkia(icon_size, SK_ColorRED)),
      icon_size, SearchResultIconShape::kCircle);
  SearchResult::SetIcon(test_icon_info);
  SearchResult::SetBadgeIcon(ui::ImageModel::FromImageSkia(
      gfx::test::CreateImageSkia(badge_icon_size, SK_ColorCYAN)));
}

}  // namespace ash
