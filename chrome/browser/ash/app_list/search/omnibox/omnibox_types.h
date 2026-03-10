// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_TYPES_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_TYPES_H_

#include <optional>
#include <string>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {

// Enum representing the Omnibox result subtype.
enum class OmniboxResultType {
  kUnset,
  kBookmark,  // A special kind of domain type.
  kDomain,
  kSearch,
  kHistory,
  kOpenTab,
};

enum class OmniboxResultAnswerType {
  kUnset,
  kDefaultAnswer,
  kWeather,
  kCurrency,
  kDictionary,
  kFinance,
  kSunrise,
  kTranslation,
  kCalculator,
};

// Enum representing special text types.
enum class OmniboxTextType {
  kUnset,
  kPositive,
  kNegative,
  kUrl,
};

struct OmniboxResultData {
  OmniboxResultData();
  OmniboxResultData(const OmniboxResultData&) = delete;
  OmniboxResultData& operator=(const OmniboxResultData&) = delete;
  ~OmniboxResultData();

  // Relevance of the result. Used for scoring/ranking.
  double relevance = 0;

  // Destination URL of the result. Used for opening the result.
  GURL destination_url;

  // Stripped destination URL of the result. This is computed from
  // |destination_url| by performing processing such as stripping off "www.",
  // converting https protocol to http, and stripping excess query parameters.
  // The stripped URL is used for unique identification and not as an actual
  // URL.
  GURL stripped_destination_url;

  // Whether the result is an omnibox search result or not.
  bool is_omnibox_search = false;

  // Whether the result is an answer result or not.
  bool is_answer = false;

  // The Omnibox search result type as required for metrics logging.
  ash::SearchResultType metrics_type = ash::SEARCH_RESULT_TYPE_BOUNDARY;

  // The Omnibox subtype of the result.
  OmniboxResultType omnibox_type = OmniboxResultType::kUnset;

  // The Omnibox answer subtype of the result.
  OmniboxResultAnswerType answer_type = OmniboxResultAnswerType::kUnset;

  // The page transition type of this result. Used for opening a result.
  ui::PageTransition page_transition = ui::PAGE_TRANSITION_TYPED;

  // The image url of the result, if any. Used to download the result image. The
  // presence of this field defines a result as a "rich entity". We consider
  // weather answer results with icons as a special kind of rich entity.
  std::optional<GURL> image_url;

  // Favicon of the result. The null-ness (gfx::ImageSkia::IsNull) of this value
  // is what defines a result as a "favicon-type result".
  gfx::ImageSkia favicon;

  // The accessibility label to use for the second image line, if one exists.
  std::optional<std::u16string> description_a11y_label;

  // The contents of the result. Used to display the result.
  std::optional<std::u16string> contents;

  // Text type of the contents, if any.
  OmniboxTextType contents_type = OmniboxTextType::kUnset;

  // Additional contents for the result. Used to display the result.
  std::optional<std::u16string> additional_contents;

  // Text type of the additional contents, if any.
  OmniboxTextType additional_contents_type = OmniboxTextType::kUnset;

  // Description of the result. Used to display the result.
  std::optional<std::u16string> description;

  // Text type of the description, if any.
  OmniboxTextType description_type = OmniboxTextType::kUnset;

  // Additional description for the result. Used to display the result.
  std::optional<std::u16string> additional_description;

  // Text type of the additional description, if any.
  OmniboxTextType additional_description_type = OmniboxTextType::kUnset;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OMNIBOX_OMNIBOX_TYPES_H_
