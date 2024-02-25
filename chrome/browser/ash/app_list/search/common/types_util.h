// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_TYPES_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_TYPES_UTIL_H_

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

// Converts result type to a debug string.
std::string ResultTypeToString(const ash::AppListSearchResultType result_type);

// Converts metrics type to a debug string.
std::string MetricsTypeToString(const ash::SearchResultType metrics_type);

// Converts display type to a debug string.
std::string DisplayTypeToString(
    const ash::SearchResultDisplayType display_type);

// Converts SearchCategory enums into ControlCategory used in the UI.
ash::AppListSearchControlCategory MapSearchCategoryToControlCategory(
    SearchCategory search_category);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_TYPES_UTIL_H_
