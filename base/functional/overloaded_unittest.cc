// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/overloaded.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace base {

TEST(FunctionalTest, Overloaded) {
  struct PackageA {};
  struct PackageB {};

  absl::variant<PackageA, PackageB> var = PackageA();

  const std::string output =
      absl::visit(Overloaded{[](const PackageA& pack) { return "PackageA"; },
                             [](const PackageB& pack) { return "PackageB"; }},
                  var);
  EXPECT_EQ(output, "PackageA");
}

}  // namespace base
