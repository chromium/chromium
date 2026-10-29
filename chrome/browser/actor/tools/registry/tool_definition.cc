// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_definition.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "components/optimization_guide/proto/features/wire/tool_declaration.pb.h"

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

optimization_guide::proto::ToolDeclaration ToolDefinition::ToProto() const {
  optimization_guide::proto::ToolDeclaration proto;
  proto.set_name(name);
  proto.set_description(description);
  const base::DictValue* properties =
      parameters_json_schema.FindDict("properties");
  if (properties && !properties->empty()) {
    std::optional<std::string> json = base::WriteJson(parameters_json_schema);
    CHECK(json);
    proto.set_parameters_json_schema(std::move(*json));
  }
  return proto;
}

}  // namespace actor
