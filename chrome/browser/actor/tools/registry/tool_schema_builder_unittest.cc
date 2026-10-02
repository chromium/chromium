// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"

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

}  // namespace
}  // namespace actor
