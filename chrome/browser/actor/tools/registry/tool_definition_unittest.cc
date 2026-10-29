// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition.h"

#include "base/values.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "components/optimization_guide/proto/features/wire/tool_declaration.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

constexpr ToolId kUnrecognizedToolId = static_cast<ToolId>(999);

TEST(ToolDefinitionTest, StoresName) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.name, "test_tool");
}

TEST(ToolDefinitionTest, ClonePreservesId) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.Clone().id, kUnrecognizedToolId);
}

TEST(ToolDefinitionTest, ClonePreservesName) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.Clone().name, "test_tool");
}

TEST(ToolDefinitionTest, ClonePreservesDescription) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.Clone().description, "Sample tool description.");
}

TEST(ToolDefinitionTest, ClonePreservesParametersJsonSchema) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.",
                     base::DictValue().Set("type", "object"));
  EXPECT_EQ(def.Clone().parameters_json_schema, def.parameters_json_schema);
}

TEST(ToolDefinitionTest, ToProtoSetsName) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.ToProto().name(), "test_tool");
}

TEST(ToolDefinitionTest, ToProtoSetsDescription) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_EQ(def.ToProto().description(), "Sample tool description.");
}

TEST(ToolDefinitionTest, ToProtoSerializesParametersJsonSchema) {
  ToolDefinition def(
      kUnrecognizedToolId, "test_tool", "Sample tool description.",
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("url",
                                     base::DictValue().Set("type", "string")))
          .Set("required", base::ListValue().Append("url")));
  EXPECT_EQ(def.ToProto().parameters_json_schema(),
            R"({"properties":{"url":{"type":"string"}},)"
            R"("required":["url"],"type":"object"})");
}

TEST(ToolDefinitionTest, ToProtoOmitsParametersJsonSchemaForEmptyDict) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.");
  EXPECT_FALSE(def.ToProto().has_parameters_json_schema());
}

TEST(ToolDefinitionTest, ToProtoOmitsParametersJsonSchemaForEmptyObjectSchema) {
  ToolDefinition def(kUnrecognizedToolId, "test_tool",
                     "Sample tool description.", ToolSchemaBuilder().Build());
  EXPECT_FALSE(def.ToProto().has_parameters_json_schema());
}

}  // namespace
}  // namespace actor
