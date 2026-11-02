// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition.h"

#include <string>
#include <type_traits>

#include "base/values.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "components/optimization_guide/proto/features/wire/tool_declaration.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

constexpr ToolId kUnrecognizedToolId = static_cast<ToolId>(999);

static_assert(
    !std::is_constructible_v<ToolDefinition, ToolId, std::string, std::string>);

TEST(ToolDefinitionTest, StoresName) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.name, "test_tool");
}

TEST(ToolDefinitionTest, ClonePreservesId) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.Clone().id, kUnrecognizedToolId);
}

TEST(ToolDefinitionTest, ClonePreservesName) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.Clone().name, "test_tool");
}

TEST(ToolDefinitionTest, ClonePreservesDescription) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.Clone().description, "Sample tool description.");
}

TEST(ToolDefinitionTest, ClonePreservesParametersJsonSchema) {
  ToolDefinition def =
      ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                            "Sample tool description.")
          .SetToolParameterSchema(
              ToolSchemaBuilder().AddStringProperty("url", "Destination URL."))
          .Build();
  EXPECT_EQ(def.Clone().parameters_json_schema, def.parameters_json_schema);
}

TEST(ToolDefinitionTest, ToProtoSetsName) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.ToProto().name(), "test_tool");
}

TEST(ToolDefinitionTest, ToProtoSetsDescription) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_EQ(def.ToProto().description(), "Sample tool description.");
}

TEST(ToolDefinitionTest, ToProtoSerializesParametersJsonSchema) {
  ToolDefinition def =
      ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                            "Sample tool description.")
          .SetToolParameterSchema(
              ToolSchemaBuilder().AddStringProperty("url", "Destination URL."))
          .Build();
  EXPECT_EQ(def.ToProto().parameters_json_schema(),
            R"({"additionalProperties":false,)"
            R"("properties":{"url":{"description":"Destination URL.",)"
            R"("type":"string"}},"required":["url"],"type":"object"})");
}

TEST(ToolDefinitionTest, ToProtoOmitsParametersJsonSchemaForEmptyObjectSchema) {
  ToolDefinition def = ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                                             "Sample tool description.")
                           .Build();
  EXPECT_FALSE(def.ToProto().has_parameters_json_schema());
}

TEST(ToolDefinitionTest,
     ToProtoOmitsParametersJsonSchemaWithoutPropertiesDict) {
  ToolDefinition def =
      ToolDefinitionBuilder(kUnrecognizedToolId, "test_tool",
                            "Sample tool description.")
          .SetToolParameterSchema(base::DictValue().Set("type", "object"))
          .Build();
  EXPECT_FALSE(def.ToProto().has_parameters_json_schema());
}

}  // namespace
}  // namespace actor
