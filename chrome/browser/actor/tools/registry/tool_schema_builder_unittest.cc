// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"

#include <string_view>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(ToolSchemaBuilderTest, EmptyBuilderProducesEmptyObjectSchema) {
  base::DictValue expected = base::DictValue()
                                 .Set("type", "object")
                                 .Set("properties", base::DictValue())
                                 .Set("additionalProperties", false);
  EXPECT_EQ(ToolSchemaBuilder().Build(), expected);
}

TEST(ToolSchemaBuilderTest, AddsStringProperty) {
  base::DictValue schema =
      ToolSchemaBuilder().AddStringProperty("query", "Search query.").Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("query",
                                     base::DictValue()
                                         .Set("type", "string")
                                         .Set("description", "Search query.")))
          .Set("required", base::ListValue().Append("query"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsFormattedStringProperty) {
  base::DictValue schema = ToolSchemaBuilder()
                               .AddStringProperty("url", "Destination URL.",
                                                  ToolSchemaBuilder::kFormatUri)
                               .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("url",
                                     base::DictValue()
                                         .Set("type", "string")
                                         .Set("description", "Destination URL.")
                                         .Set("format", "uri")))
          .Set("required", base::ListValue().Append("url"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsStringEnumProperty) {
  constexpr std::string_view kDirections[] = {"up", "down", "left", "right"};
  base::DictValue schema =
      ToolSchemaBuilder()
          .AddStringEnumProperty("direction", "Scroll direction.", kDirections)
          .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "direction", base::DictValue()
                                    .Set("type", "string")
                                    .Set("description", "Scroll direction.")
                                    .Set("enum", base::ListValue()
                                                     .Append("up")
                                                     .Append("down")
                                                     .Append("left")
                                                     .Append("right"))))
          .Set("required", base::ListValue().Append("direction"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsIntegerProperty) {
  base::DictValue schema =
      ToolSchemaBuilder().AddIntegerProperty("seek_ms", "Seek offset.").Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("seek_ms",
                                     base::DictValue()
                                         .Set("type", "integer")
                                         .Set("description", "Seek offset.")))
          .Set("required", base::ListValue().Append("seek_ms"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsNumberProperty) {
  base::DictValue schema = ToolSchemaBuilder()
                               .AddNumberProperty("distance", "Scroll pixels.")
                               .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set("distance",
                                     base::DictValue()
                                         .Set("type", "number")
                                         .Set("description", "Scroll pixels.")))
          .Set("required", base::ListValue().Append("distance"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsBooleanProperty) {
  base::DictValue schema =
      ToolSchemaBuilder()
          .AddBooleanProperty("submit", "Whether to submit.")
          .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "submit", base::DictValue()
                                 .Set("type", "boolean")
                                 .Set("description", "Whether to submit.")))
          .Set("required", base::ListValue().Append("submit"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsIntegerArrayProperty) {
  base::DictValue schema =
      ToolSchemaBuilder()
          .AddArrayProperty("ids", "The IDs.",
                            ToolSchemaBuilder::ArrayItemType::kInteger)
          .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "ids",
                   base::DictValue()
                       .Set("type", "array")
                       .Set("description", "The IDs.")
                       .Set("items", base::DictValue().Set("type", "integer"))))
          .Set("required", base::ListValue().Append("ids"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

TEST(ToolSchemaBuilderTest, AddsStringArrayProperty) {
  base::DictValue schema =
      ToolSchemaBuilder()
          .AddArrayProperty("urls", "The URLs.",
                            ToolSchemaBuilder::ArrayItemType::kString)
          .Build();
  base::DictValue expected =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue().Set(
                   "urls",
                   base::DictValue()
                       .Set("type", "array")
                       .Set("description", "The URLs.")
                       .Set("items", base::DictValue().Set("type", "string"))))
          .Set("required", base::ListValue().Append("urls"))
          .Set("additionalProperties", false);
  EXPECT_EQ(schema, expected);
}

}  // namespace
}  // namespace actor
