// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_registry.h"

#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/actor/tool_request_variant.h"

namespace actor {

namespace {

struct ToolCatalog {
  std::vector<ToolDefinition> tools;
  base::flat_map<std::string_view, ToolId> name_to_id;
  base::flat_map<ToolId, std::string_view> id_to_name;
};

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

ToolCatalog BuildToolCatalog() {
  ToolCatalog catalog;
  catalog.tools =
      BuildToolDefinitionsForVariant(std::type_identity<ToolRequestVariant>{});

  std::vector<std::pair<std::string_view, ToolId>> name_to_id_entries;
  std::vector<std::pair<ToolId, std::string_view>> id_to_name_entries;
  name_to_id_entries.reserve(catalog.tools.size());
  id_to_name_entries.reserve(catalog.tools.size());

  for (const ToolDefinition& tool : catalog.tools) {
    CHECK(!tool.name.empty());
    name_to_id_entries.emplace_back(tool.name, tool.id);
    id_to_name_entries.emplace_back(tool.id, tool.name);
  }

  catalog.name_to_id =
      base::flat_map<std::string_view, ToolId>(std::move(name_to_id_entries));
  catalog.id_to_name =
      base::flat_map<ToolId, std::string_view>(std::move(id_to_name_entries));

  // Ensure all names and IDs are unique.
  CHECK_EQ(catalog.name_to_id.size(), catalog.tools.size());
  CHECK_EQ(catalog.id_to_name.size(), catalog.tools.size());

  return catalog;
}

const ToolCatalog& GetStaticToolCatalog() {
  static const base::NoDestructor<ToolCatalog> kCatalog(BuildToolCatalog());
  return *kCatalog;
}

}  // namespace

ToolRegistry::ToolRegistry() {
  // Ensure the static tool catalog is initialized once at service creation.
  GetStaticToolCatalog();
}

ToolRegistry::~ToolRegistry() = default;

std::optional<ToolId> ToolRegistry::NameToToolId(std::string_view name) {
  const auto& name_to_id = GetStaticToolCatalog().name_to_id;
  auto it = name_to_id.find(name);
  if (it == name_to_id.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::string_view ToolRegistry::ToolIdToName(ToolId id) {
  const auto& id_to_name = GetStaticToolCatalog().id_to_name;
  auto it = id_to_name.find(id);
  if (it == id_to_name.end()) {
    return "";
  }
  return it->second;
}

base::span<const ToolDefinition> ToolRegistry::GetAllTools() const {
  return GetStaticToolCatalog().tools;
}

std::vector<const ToolDefinition*> ToolRegistry::GetToolsByIds(
    const std::set<ToolId>& ids) const {
  std::vector<const ToolDefinition*> matching_tools;
  for (const ToolDefinition& tool : GetStaticToolCatalog().tools) {
    if (ids.contains(tool.id)) {
      matching_tools.push_back(&tool);
    }
  }
  return matching_tools;
}

}  // namespace actor
