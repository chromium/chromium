// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"

#include <tuple>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

constexpr ToolId kTestToolId = static_cast<ToolId>(999);

TEST(ToolDefinitionBuilderTest, BuildSetsId) {
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .Build()
          .id,
      kTestToolId);
}

TEST(ToolDefinitionBuilderTest, BuildSetsName) {
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .Build()
          .name,
      "test_tool");
}

TEST(ToolDefinitionBuilderTest, BuildSetsDescription) {
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .Build()
          .description,
      "Test tool description.");
}

TEST(ToolDefinitionBuilderTest, BuildWithoutParametersProducesEmptySchema) {
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .Build()
          .parameters_json_schema,
      ToolSchemaBuilder().Build());
}

TEST(ToolDefinitionBuilderTest,
     SetToolParameterSchemaAcceptsToolSchemaBuilder) {
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .SetToolParameterSchema(
              ToolSchemaBuilder().AddStringProperty("query", "Search query."))
          .Build()
          .parameters_json_schema,
      ToolSchemaBuilder().AddStringProperty("query", "Search query.").Build());
}

TEST(ToolDefinitionBuilderTest, SetToolParameterSchemaAcceptsDictValue) {
  base::DictValue custom_schema;
  custom_schema.Set("type", "object");
  EXPECT_EQ(
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .SetToolParameterSchema(custom_schema.Clone())
          .Build()
          .parameters_json_schema,
      custom_schema);
}

TEST(ToolDefinitionBuilderTest,
     SetToolParameterSchemaChainedParametersAreAllRequiredInOrder) {
  base::DictValue schema =
      ToolDefinitionBuilder(kTestToolId, "test_tool", "Test tool description.")
          .SetToolParameterSchema(
              ToolSchemaBuilder()
                  .AddStringProperty("query", "Search query.")
                  .AddIntegerProperty("limit", "Result limit."))
          .Build()
          .parameters_json_schema;
  const base::ListValue* required = schema.FindList("required");
  ASSERT_TRUE(required);
  EXPECT_THAT(*required, testing::ElementsAre("query", "limit"));
}

TEST(ToolDefinitionBuilderDeathTest, EmptyNameCrashes) {
  EXPECT_CHECK_DEATH(std::ignore = ToolDefinitionBuilder(
                         kTestToolId, "", "Test tool description."));
}

TEST(ToolDefinitionBuilderDeathTest, EmptyDescriptionCrashes) {
  EXPECT_CHECK_DEATH(std::ignore =
                         ToolDefinitionBuilder(kTestToolId, "test_tool", ""));
}

}  // namespace
}  // namespace actor
