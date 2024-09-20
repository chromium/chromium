// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base {

PA_CONSTINIT static NoDestructor<int> an_int;

static auto& GetVector() {
  static NoDestructor<std::vector<int>> a_vec({1, 2, 3});
  return *a_vec;
}

TEST(NoDestructorTest, Basic) {
  // Should always be the same instance.
  auto* instance = &GetVector();
  ASSERT_EQ(instance, &GetVector());

  EXPECT_THAT(*instance, ::testing::ElementsAre(1, 2, 3));

  EXPECT_EQ(0, *an_int);
}

}  // namespace partition_alloc::internal::base
