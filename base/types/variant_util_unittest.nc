// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/variant_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

#if defined(NCTEST_DUPLICATE_ALTERNATIVE_TYPES)  // [r"Variant is not constructible from T"]

inline constexpr size_t kValue = VariantIndexOfType<absl::variant<int, int>, int>();

#elif defined(NCTEST_NOT_AN_ALTERNATIVE_TYPE)  // [r"Variant is not constructible from T"]

inline constexpr size_t kValue = VariantIndexOfType<absl::variant<int, int>, bool>();

#endif

}  // namespace base
