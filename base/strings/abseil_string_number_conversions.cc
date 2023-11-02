// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_number_conversions.h"

#include "base/strings/string_number_conversions_internal.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace base {

bool StringToUint128(StringPiece input, absl::uint128* output) {
  return internal::StringToIntImpl(input, *output);
}

bool HexStringToUInt128(StringPiece input, absl::uint128* output) {
  return internal::HexStringToIntImpl(input, *output);
}

}  // namespace base
