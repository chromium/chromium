// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_number_conversions.h"

#include <string_view>

#include "base/strings/string_number_conversions_internal.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

namespace base {

bool StringToUint128(std::string_view input, absl::uint128* output) {
  return internal::StringToIntImpl(input, *output);
}

bool HexStringToUInt128(std::string_view input, absl::uint128* output) {
  return internal::HexStringToIntImpl(input, *output);
}

}  // namespace base
