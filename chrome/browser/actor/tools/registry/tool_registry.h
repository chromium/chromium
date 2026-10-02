// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_REGISTRY_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_REGISTRY_H_

#include <set>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"

namespace actor {

// Registry of shared browser tools exposed by ActorKeyedService.
class ToolRegistry {
 public:
  ToolRegistry();
  ~ToolRegistry();

  ToolRegistry(const ToolRegistry&) = delete;
  ToolRegistry& operator=(const ToolRegistry&) = delete;

  // Returns all registered Actor browser tool definitions.
  base::span<const ToolDefinition> GetAllTools() const;

  // Returns pointers to the registered tool definitions whose `ToolId` is in
  // `ids`. Unrecognized IDs in `ids` are ignored.
  std::vector<const ToolDefinition*> GetToolsByIds(
      const std::set<ToolId>& ids) const;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_REGISTRY_H_
