// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_

#include <string>

#include "components/optimization_guide/proto/features/finds.pb.h"

namespace finds {

// Converts a FindsSuggestionResponse::SuggestionTheme::ThemeType proto enum to
// its corresponding string representation used in preference names. Returns
// an empty string if the theme type is unknown.
std::string ThemeTypeEnumToString(
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::
        ThemeType theme_type);

}  // namespace finds

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_UTILS_H_
