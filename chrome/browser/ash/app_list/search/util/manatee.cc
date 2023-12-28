// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/manatee.h"

#include <numeric>
#include <optional>
#include <vector>

namespace app_list {

std::optional<double> GetWordSimilarity(const std::vector<double>& vector1,
                                        const std::vector<double>& vector2) {
  if (vector1.size() != vector2.size()) {
    return std::nullopt;
  }
  return std::inner_product(vector1.begin(), vector1.end(), vector2.begin(), 0);
}

}  // namespace app_list
