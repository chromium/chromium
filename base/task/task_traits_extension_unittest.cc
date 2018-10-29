// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_traits.h"

#include "base/task/test_task_traits_extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(TaskTraitsExtensionTest, NoExtension) {
  constexpr TaskTraits traits = {};

  EXPECT_EQ(traits.extension_id(),
            TaskTraitsExtensionStorage::kInvalidExtensionId);
}

TEST(TaskTraitsExtensionTest, CreateWithOneExtensionTrait) {
  constexpr TaskTraits traits = {TestExtensionEnumTrait::kB};

  EXPECT_EQ(traits.GetExtension<TestTaskTraitsExtension>().enum_trait(),
            TestExtensionEnumTrait::kB);
  EXPECT_FALSE(traits.GetExtension<TestTaskTraitsExtension>().bool_trait());
}

TEST(TaskTraitsExtensionTest, CreateWithMultipleExtensionTraits) {
  constexpr TaskTraits traits = {TestExtensionEnumTrait::kB,
                                 TestExtensionBoolTrait()};

  EXPECT_EQ(traits.GetExtension<TestTaskTraitsExtension>().enum_trait(),
            TestExtensionEnumTrait::kB);
  EXPECT_TRUE(traits.GetExtension<TestTaskTraitsExtension>().bool_trait());
}

TEST(TaskTraitsExtensionTest, CreateWithBaseAndExtensionTraits) {
  constexpr TaskTraits traits = {TaskPriority::USER_BLOCKING,
                                 TestExtensionEnumTrait::kC,
                                 TestExtensionBoolTrait()};

  EXPECT_EQ(traits.priority(), TaskPriority::USER_BLOCKING);
  EXPECT_EQ(traits.GetExtension<TestTaskTraitsExtension>().enum_trait(),
            TestExtensionEnumTrait::kC);
  EXPECT_TRUE(traits.GetExtension<TestTaskTraitsExtension>().bool_trait());
}

}  // namespace base
