// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_MANATEE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_MANATEE_H_

#include <optional>
#include <vector>

namespace app_list {

// Returns cosine similarity between two vectors. It requires that both
// vectors be the same size, and returns std::nullopt if not. Values can
// range between [0, 1] where a higher value indicates a greater
// similarity.
std::optional<double> GetEmbeddingSimilarity(
    const std::vector<double>& vector1,
    const std::vector<double>& vector2);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_MANATEE_H_
