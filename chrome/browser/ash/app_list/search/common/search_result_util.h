// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_SEARCH_RESULT_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_SEARCH_RESULT_UTIL_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"

namespace app_list {

// Returns a kString text item containing the localized string with the given
// GRIT ID.
ash::SearchResultTextItem CreateStringTextItem(int text_id);

// Returns a kString text item containing `text`.
ash::SearchResultTextItem CreateStringTextItem(const std::u16string& text);

// Returns a kIconifiedText text item containing `text`.
ash::SearchResultTextItem CreateIconifiedTextTextItem(
    const std::u16string& text);

// Returns a kIcon text item with the given text icon code.
ash::SearchResultTextItem CreateIconCodeTextItem(
    ash::SearchResultTextItem::IconCode icon_code);

// Given a `text_vector`, such as a ChromeSearchResult's title or details,
// returns a 'flattened' string that is the concatenation of all text in
// `text_vector` without formatting. Requires that all elements of
// `text_vector` are kString items.
std::u16string StringFromTextVector(
    const std::vector<ash::SearchResultTextItem>& text_vector);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_SEARCH_RESULT_UTIL_H_
