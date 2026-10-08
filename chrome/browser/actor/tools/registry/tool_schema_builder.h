// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_SCHEMA_BUILDER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_SCHEMA_BUILDER_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/values.h"

namespace actor {

// Helper for building a tool's JSON Schema parameter object dictionary.
//
// ToolSchemaBuilder intentionally does not support optional parameters: every
// added property is marked "required". Actor tool definitions require
// unconditional parameters, so an action with multiple modes should be split
// into separate 1:1 ToolRequest subclasses rather than exposing optional
// parameters.
class ToolSchemaBuilder {
 public:
  // JSON Schema string format annotation for URI values.
  static constexpr std::string_view kFormatUri = "uri";

  ToolSchemaBuilder();
  ~ToolSchemaBuilder();
  ToolSchemaBuilder(const ToolSchemaBuilder&) = delete;
  ToolSchemaBuilder& operator=(const ToolSchemaBuilder&) = delete;

  // Adds a required string property to the schema, optionally annotated with
  // `format` (e.g. `kFormatUri`).
  ToolSchemaBuilder& AddStringProperty(
      std::string_view name,
      std::string_view description,
      std::optional<std::string_view> format = std::nullopt);

  // Adds a required string property constrained to `enum_values` to the schema.
  // `enum_values` must be non-empty.
  ToolSchemaBuilder& AddStringEnumProperty(
      std::string_view name,
      std::string_view description,
      base::span<const std::string_view> enum_values);

  // Adds a required integer property to the schema.
  ToolSchemaBuilder& AddIntegerProperty(std::string_view name,
                                        std::string_view description);

  // Adds a required number property to the schema.
  ToolSchemaBuilder& AddNumberProperty(std::string_view name,
                                       std::string_view description);

  // Adds a required boolean property to the schema.
  ToolSchemaBuilder& AddBooleanProperty(std::string_view name,
                                        std::string_view description);

  // Consumes the accumulated properties and returns the top-level JSON Schema
  // object dictionary. Call at most once: `Build()` moves out the builder's
  // state, so the builder must not be reused afterwards.
  [[nodiscard]] base::DictValue Build();

 private:
  // Inserts `prop_schema` under `name` in `properties_` and records `name` in
  // `required_`.
  void AddPropertyImpl(std::string_view name, base::DictValue prop_schema);

  // Map from parameter name to its individual JSON Schema property definition.
  base::DictValue properties_;

  // Ordered list of required parameter names for the top-level object schema.
  base::ListValue required_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_SCHEMA_BUILDER_H_
