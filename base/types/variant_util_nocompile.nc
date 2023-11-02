// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/types/variant_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

// Cannot get the index of a type if the type is specified multiple times in
// the variant instantiation.
inline constexpr size_t kValue = VariantIndexOfType<absl::variant<int, int>, int>();  // expected-error {{constexpr variable 'kValue' must be initialized by a constant expression}}
                                                                                      // expected-error@base/types/variant_util.h:* {{Variant is not constructible from T}}
                                                                                      // expected-error@base/types/variant_util.h:* {{no matching conversion for functional-style cast}}

// Should fail if the type is not mentioned in the variant instantiation at
// all.
inline constexpr size_t kValue2 = VariantIndexOfType<absl::variant<int>, bool>();  // expected-error {{constexpr variable 'kValue2' must be initialized by a constant expression}}
                                                                                   // expected-error@base/types/variant_util.h:* {{Variant is not constructible from T}}
                                                                                   // expected-error@base/types/variant_util.h:* {{no matching conversion for functional-style cast}}

}  // namespace base
