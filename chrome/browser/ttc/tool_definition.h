// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TOOL_DEFINITION_H_
#define CHROME_BROWSER_TTC_TOOL_DEFINITION_H_

#include <string>

#include "base/values.h"

namespace ttc {

// Defines a tool registered with the model execution backend.
struct ToolDefinition {
  // Behavior specifies whether model execution should block waiting for tool
  // results or continue asynchronously.
  enum class Behavior {
    // Model execution blocks until tool execution completes and returns
    // results.
    kBlocking,
    // Model execution continues without blocking on tool execution results.
    kNonBlocking,
  };

  // Verbalization specifies whether the model should speak or describe its
  // action to the user while/after executing the tool.
  enum class Verbalization {
    // Standard verbalization where the model can speak or acknowledge tool
    // actions.
    kStandard,
    // Silent action where the tool executes without spoken verbalization from
    // the model.
    kSilentAction,
  };

  ToolDefinition();
  ~ToolDefinition();
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);
  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;

  std::string name;
  std::string description;
  base::DictValue parameters_json_schema;
  Behavior behavior = Behavior::kBlocking;
  Verbalization verbalization = Verbalization::kStandard;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TOOL_DEFINITION_H_
