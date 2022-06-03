// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_conversions.h"

#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace base {

std::vector<absl::string_view> StringPiecesToStringViews(
    span<const StringPiece> pieces) {
  std::vector<absl::string_view> views(pieces.size());
  ranges::transform(pieces, views.begin(), &StringPieceToStringView);
  return views;
}

std::vector<StringPiece> StringViewsToStringPieces(
    span<const absl::string_view> views) {
  std::vector<StringPiece> pieces(views.size());
  ranges::transform(views, pieces.begin(), &StringViewToStringPiece);
  return pieces;
}

}  // namespace base
