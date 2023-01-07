// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/functional/overloaded.h"

#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

#if defined(TEST_LAMDA_MISSING_FOR_A_VARIANT_OPTION)  // [r"error: no type named 'type' in 'absl::type_traits_internal::result_of<base::Overloaded<\(lambda at *\)> \(PackageB &\)>'"]
  struct PackageA {};
  struct PackageB {};

  absl::variant<PackageA, PackageB> var = PackageA();
  absl::visit(
      Overloaded{[](PackageA& pack) { return "PackageA"; }},
      var);

#endif

}  // namespace base