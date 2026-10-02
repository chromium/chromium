// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/registry/tool_registry.h"

#include <vector>

#include "base/no_destructor.h"

namespace actor {

namespace {

std::vector<ToolDefinition> BuildAllToolDefinitions() {
  std::vector<ToolDefinition> tools;
  return tools;
}

const std::vector<ToolDefinition>& GetStaticToolCatalog() {
  static const base::NoDestructor<std::vector<ToolDefinition>> kCatalog(
      BuildAllToolDefinitions());
  return *kCatalog;
}

}  // namespace

ToolRegistry::ToolRegistry() = default;

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
