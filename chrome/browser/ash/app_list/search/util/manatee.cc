// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/manatee.h"

#include <numeric>
#include <optional>
#include <vector>

namespace app_list {

std::optional<double> GetEmbeddingSimilarity(
    const std::vector<double>& vector1,
    const std::vector<double>& vector2) {
  if (vector1.size() != vector2.size()) {
    return std::nullopt;
  }
  // Calculating cosine similarity.
  double inner_prod =
      std::inner_product(vector1.begin(), vector1.end(), vector2.begin(), 0.0);

  double magnitude1 = std::sqrt(
      std::inner_product(vector1.begin(), vector1.end(), vector1.begin(), 0.0));
  double magnitude2 = std::sqrt(
      std::inner_product(vector2.begin(), vector2.end(), vector2.begin(), 0.0));

  if (magnitude1 == 0.0 || magnitude2 == 0.0) {
    return 0.0;
  }

  double cos_sim = inner_prod / (magnitude1 * magnitude2);
  // Map to range [0, 1] from [-1, 1]
  return (1.0 + cos_sim) / 2.0;
}

}  // namespace app_list
