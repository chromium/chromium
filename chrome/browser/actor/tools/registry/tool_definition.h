// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
#define CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_

#include <string>

#include "base/values.h"

namespace actor {

// Identifier for each shared browser tool in ActorKeyedService.
//
// These values may be persisted to logs. Entries are append-only: do not
// renumber or reuse numeric values, and update `kMaxValue` when adding new
// entries.
enum class ToolId {
  // Navigates the active tab to a specified URL.
  kNavigate = 0,
  // Clicks an element on the active page by DOM node ID.
  kClick = 1,
  // Switches to an open tab in the active browser window matching a query.
  kSwitchTab = 2,
  // Scrolls an element or the main viewport in the active tab.
  kScroll = 3,
  // Selects an option in a dropdown (<select>) element on the page.
  kSelectOption = 4,
  // Types text into an editable element on the page.
  kType = 5,
  // Translates the current page.
  kTranslatePage = 6,
  // Navigates the active tab backward one entry in session history.
  kGoBack = 7,
  // Navigates the active tab forward one entry in session history.
  kGoForward = 8,
  // Reloads the current page in the active tab.
  kReloadPage = 9,
  // Seeks to a specific timestamp in the media in the active tab.
  kSeekToTimestamp = 10,
  kMaxValue = kSeekToTimestamp,
};

// In-memory metadata and parameter schema for a shared browser tool. Prefer
// building instances with `ToolDefinitionBuilder`.
struct ToolDefinition {
  ToolDefinition(ToolId id, std::string name, std::string description);
  ToolDefinition(ToolId id,
                 std::string name,
                 std::string description,
                 base::DictValue parameters_json_schema);
  ~ToolDefinition();

  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);

  // Needed to make a copy of `ToolDefinition`, because `base::DictValue` is
  // move-only.
  ToolDefinition Clone() const;

  // Chrome-internal identifier for this tool.
  ToolId id;

  // Model-facing name of this tool (e.g. "navigate").
  std::string name;

  // Client-side description of what this tool does and when to invoke it.
  std::string description;

  // JSON Schema object dictionary describing the tool's input parameters.
  base::DictValue parameters_json_schema;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_REGISTRY_TOOL_DEFINITION_H_
