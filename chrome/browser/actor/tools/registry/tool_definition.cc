// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition.h"

#include <utility>

namespace actor {

ToolDefinition::ToolDefinition(ToolId id,
                               std::string name,
                               std::string description)
    : ToolDefinition(id,
                     std::move(name),
                     std::move(description),
                     base::DictValue()) {}

ToolDefinition::ToolDefinition(ToolId id,
                               std::string name,
                               std::string description,
                               base::DictValue parameters_json_schema)
    : id(id),
      name(std::move(name)),
      description(std::move(description)),
      parameters_json_schema(std::move(parameters_json_schema)) {}

ToolDefinition::~ToolDefinition() = default;

ToolDefinition::ToolDefinition(ToolDefinition&&) = default;

ToolDefinition& ToolDefinition::operator=(ToolDefinition&&) = default;

ToolDefinition ToolDefinition::Clone() const {
  return ToolDefinition(id, name, description, parameters_json_schema.Clone());
}

}  // namespace actor
