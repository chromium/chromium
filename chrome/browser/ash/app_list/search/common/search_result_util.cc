// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/search_result_util.h"

#include "base/strings/strcat.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace {

using TextType = ash::SearchResultTextItemType;

}  //  namespace

ash::SearchResultTextItem CreateStringTextItem(int text_id) {
  return CreateStringTextItem(l10n_util::GetStringUTF16(text_id));
}

ash::SearchResultTextItem CreateStringTextItem(const std::u16string& text) {
  ash::SearchResultTextItem text_item(TextType::kString);
  text_item.SetText(text);
  text_item.SetTextTags({});
  return text_item;
}

ash::SearchResultTextItem CreateIconifiedTextTextItem(
    const std::u16string& text) {
  ash::SearchResultTextItem text_item(TextType::kIconifiedText);
  text_item.SetText(text);
  text_item.SetTextTags({});
  return text_item;
}

ash::SearchResultTextItem CreateIconCodeTextItem(
    const ash::SearchResultTextItem::IconCode icon_code) {
  ash::SearchResultTextItem text_item(TextType::kIconCode);
  text_item.SetIconCode(icon_code);
  return text_item;
}

std::u16string StringFromTextVector(
    const std::vector<ash::SearchResultTextItem>& text_vector) {
  std::vector<std::u16string> result;
  for (const auto& text_item : text_vector) {
    DCHECK_EQ(text_item.GetType(), TextType::kString);
    result.push_back(text_item.GetText());
  }
  return base::StrCat(result);
}

}  // namespace app_list
