// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"

#include <utility>

#include "base/check.h"

namespace actor {

ToolDefinitionBuilder::ToolDefinitionBuilder(ToolId id,
                                             std::string_view name,
                                             std::string_view description)
    : id_(id), name_(name), description_(description) {
  CHECK(!name_.empty());
  CHECK(!description_.empty());
}

ToolDefinitionBuilder::~ToolDefinitionBuilder() = default;

ToolDefinitionBuilder& ToolDefinitionBuilder::SetToolParameterSchema(
    ToolSchemaBuilder& schema_builder) {
  parameters_json_schema_ = schema_builder.Build();
  return *this;
}

ToolDefinitionBuilder& ToolDefinitionBuilder::SetToolParameterSchema(
    base::DictValue schema) {
  parameters_json_schema_ = std::move(schema);
  return *this;
}

ToolDefinition ToolDefinitionBuilder::Build() {
  return ToolDefinition(id_, std::move(name_), std::move(description_),
                        parameters_json_schema_.has_value()
                            ? std::move(*parameters_json_schema_)
                            : ToolSchemaBuilder().Build());
}

}  // namespace actor
