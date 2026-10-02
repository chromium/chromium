// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_registry.h"

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/actor/tool_request_variant.h"

namespace actor {

namespace {

template <typename T>
void AppendToolDefinitionIfPresent(std::vector<ToolDefinition>& tools) {
  if (std::optional<ToolDefinition> tool = T::GetToolDefinition()) {
    tools.push_back(std::move(*tool));
  }
}

template <typename... Ts>
std::vector<ToolDefinition> BuildToolDefinitionsForVariant(
    std::type_identity<std::variant<Ts...>>) {
  std::vector<ToolDefinition> tools;
  (AppendToolDefinitionIfPresent<Ts>(tools), ...);
  return tools;
}

std::vector<ToolDefinition> BuildAllToolDefinitions() {
  return BuildToolDefinitionsForVariant(
      std::type_identity<ToolRequestVariant>{});
}

const std::vector<ToolDefinition>& GetStaticToolCatalog() {
  static const base::NoDestructor<std::vector<ToolDefinition>> kCatalog(
      BuildAllToolDefinitions());
  return *kCatalog;
}

}  // namespace

ToolRegistry::ToolRegistry() {
  // Ensure the static tool catalog is initialized once at service creation.
  GetStaticToolCatalog();
}

ToolRegistry::~ToolRegistry() = default;

base::span<const ToolDefinition> ToolRegistry::GetAllTools() const {
  return GetStaticToolCatalog();
}

std::vector<const ToolDefinition*> ToolRegistry::GetToolsByIds(
    const std::set<ToolId>& ids) const {
  std::vector<const ToolDefinition*> matching_tools;
  for (const ToolDefinition& tool : GetStaticToolCatalog()) {
    if (ids.contains(tool.id)) {
      matching_tools.push_back(&tool);
    }
  }
  return matching_tools;
}

}  // namespace actor
