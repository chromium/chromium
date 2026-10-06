// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"

namespace actor {

// Identifier for each shared browser tool in ActorKeyedService.
//
// These values may be persisted to logs. Entries are append-only: do not
// renumber or reuse numeric values, and update `kMaxValue` when adding new
// entries.
enum class ToolId {};

// Converts `id` to its canonical model-facing tool name, or returns an empty
// string_view if `id` is unrecognized.
std::string_view ToolIdToName(ToolId id);

// Resolves a model-facing tool `name` to its corresponding `ToolId`, or
// returns `std::nullopt` if `name` is not a registered Actor tool.
std::optional<ToolId> NameToToolId(std::string_view name);

// In-memory metadata and parameter schema for a shared browser tool. Prefer
// building instances with `ToolDefinitionBuilder`.
struct ToolDefinition {
  ToolDefinition(ToolId id, std::string description);
  ToolDefinition(ToolId id,
                 std::string description,
                 base::DictValue parameters_json_schema);
  ~ToolDefinition();

  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);

  // Derived directly from `id` via `ToolIdToName(id)`.
  std::string_view name() const { return ToolIdToName(id); }

  // Needed to make a copy of `ToolDefinition`, because `base::DictValue` is
  // move-only.
  ToolDefinition Clone() const;

  // Chrome-internal identifier for this tool.
  ToolId id;

  // Client-side description of what this tool does and when to invoke it.
  std::string description;

  // JSON Schema object dictionary describing the tool's input parameters.
  base::DictValue parameters_json_schema;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
