// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

class AIUtilsTest : public testing::Test {};

TEST_F(AIUtilsTest, ToMojomResponseConstraintValidRegex) {
  optimization_guide::proto::ResponseConstraint constraint_proto;
  constraint_proto.set_regex("^Result: .*");

  auto mojom_constraint = ai::ToMojomResponseConstraint(constraint_proto);
  EXPECT_TRUE(mojom_constraint);
  EXPECT_TRUE(mojom_constraint->is_regex());
  EXPECT_EQ(mojom_constraint->get_regex(), "^Result: .*");
}

TEST_F(AIUtilsTest, ToMojomResponseConstraintValidJsonSchema) {
  optimization_guide::proto::ResponseConstraint constraint_proto;
  constraint_proto.set_json_schema("{}");

  auto mojom_constraint = ai::ToMojomResponseConstraint(constraint_proto);
  EXPECT_TRUE(mojom_constraint);
  EXPECT_TRUE(mojom_constraint->is_json_schema());
  EXPECT_EQ(mojom_constraint->get_json_schema(), "{}");
}

TEST_F(AIUtilsTest, ToMojomResponseConstraintNoConstraint) {
  optimization_guide::proto::ResponseConstraint constraint_proto;

  auto mojom_constraint = ai::ToMojomResponseConstraint(constraint_proto);
  EXPECT_FALSE(mojom_constraint);
}
