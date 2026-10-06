// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition_test_util.h"

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

using ::testing::Not;

constexpr ToolId kTestToolId = static_cast<ToolId>(999);

ToolDefinition BuildDefinitionWithQueryAndCount() {
  return ToolDefinitionBuilder(kTestToolId, "Test tool description.")
      .SetToolParameterSchema(ToolSchemaBuilder()
                                  .AddStringProperty("query", "Search query.")
                                  .AddIntegerProperty("count", "Result count."))
      .Build();
}

ToolDefinition BuildDefinitionWithoutParameters() {
  return ToolDefinitionBuilder(kTestToolId, "Test tool description.").Build();
}

TEST(ToolDefinitionTestUtilTest, HasParamOfTypeMatchesDeclaredStringParam) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(),
              HasParamOfType("query", "string"));
}

TEST(ToolDefinitionTestUtilTest, HasParamOfTypeMatchesDeclaredIntegerParam) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(),
              HasParamOfType("count", "integer"));
}

TEST(ToolDefinitionTestUtilTest, HasParamOfTypeRejectsWrongType) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(),
              Not(HasParamOfType("query", "integer")));
}

TEST(ToolDefinitionTestUtilTest, HasParamOfTypeRejectsUndeclaredParam) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(),
              Not(HasParamOfType("missing", "string")));
}

TEST(ToolDefinitionTestUtilTest, HasParamOfTypeRejectsEmptySchema) {
  EXPECT_THAT(BuildDefinitionWithoutParameters(),
              Not(HasParamOfType("query", "string")));
}

TEST(ToolDefinitionTestUtilTest, RequiresParamMatchesRequiredParam) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(), RequiresParam("count"));
}

TEST(ToolDefinitionTestUtilTest, RequiresParamRejectsUndeclaredParam) {
  EXPECT_THAT(BuildDefinitionWithQueryAndCount(),
              Not(RequiresParam("missing")));
}

TEST(ToolDefinitionTestUtilTest, RequiresParamRejectsEmptySchema) {
  EXPECT_THAT(BuildDefinitionWithoutParameters(), Not(RequiresParam("query")));
}

}  // namespace
}  // namespace actor
