// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_BUILDER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_BUILDER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"

namespace actor {

// Fluent builder for a complete `ToolDefinition`. Each `ToolRequest` subclass
// uses it to declare its tool locally, e.g.:
//
//   ToolDefinitionBuilder(ToolId::kFoo, "foo", "Does foo to the given URL.")
//       .SetToolParameterSchema(
//           ToolSchemaBuilder().AddStringProperty(
//               "url", "The URL to foo.", ToolSchemaBuilder::kFormatUri))
//       .Build();
//
// Every parameter is required (see `ToolSchemaBuilder`).
class ToolDefinitionBuilder {
 public:
  ToolDefinitionBuilder(ToolId id,
                        std::string_view name,
                        std::string_view description);
  ~ToolDefinitionBuilder();
  ToolDefinitionBuilder(const ToolDefinitionBuilder&) = delete;
  ToolDefinitionBuilder& operator=(const ToolDefinitionBuilder&) = delete;

  // Sets the tool's JSON Schema parameter object definition. If not called,
  // defaults to an empty object schema (no parameters).
  ToolDefinitionBuilder& SetToolParameterSchema(
      ToolSchemaBuilder& schema_builder);
  ToolDefinitionBuilder& SetToolParameterSchema(base::DictValue schema);

  // Consumes the accumulated state and returns the `ToolDefinition`. Call at
  // most once: the builder must not be reused afterwards.
  [[nodiscard]] ToolDefinition Build();

 private:
  const ToolId id_;
  std::string name_;
  std::string description_;
  std::optional<base::DictValue> parameters_json_schema_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_BUILDER_H_
