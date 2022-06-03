// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_ABSEIL_STRING_CONVERSIONS_H_
#define BASE_STRINGS_ABSEIL_STRING_CONVERSIONS_H_

#include <vector>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace base {

// Converts `piece` to a string view, pointing to the same piece of memory.
constexpr absl::string_view StringPieceToStringView(StringPiece piece) {
  return {piece.data(), piece.size()};
}

// Converts `view` to a string piece, pointing to the same piece of memory.
constexpr StringPiece StringViewToStringPiece(absl::string_view view) {
  return {view.data(), view.size()};
}

// Converts `pieces` to string views, pointing to the same piece of memory.
BASE_EXPORT std::vector<absl::string_view> StringPiecesToStringViews(
    span<const StringPiece> pieces);

// Converts `views` to string pieces, pointing to the same piece of memory.
BASE_EXPORT std::vector<StringPiece> StringViewsToStringPieces(
    span<const absl::string_view> views);

}  // namespace base

#endif  // BASE_STRINGS_ABSEIL_STRING_CONVERSIONS_H_
