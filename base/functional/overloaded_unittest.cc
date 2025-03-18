// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/overloaded.h"

#include <string>
#include <variant>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FunctionalTest, Overloaded) {
  struct PackageA {};
  struct PackageB {};

  std::variant<PackageA, PackageB> var = PackageA();

  const std::string output =
      std::visit(Overloaded{[](const PackageA& pack) { return "PackageA"; },
                            [](const PackageB& pack) { return "PackageB"; }},
                 var);
  EXPECT_EQ(output, "PackageA");
}

}  // namespace base
