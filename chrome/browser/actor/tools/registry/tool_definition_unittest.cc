// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition.h"

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

}  // namespace
}  // namespace actor
