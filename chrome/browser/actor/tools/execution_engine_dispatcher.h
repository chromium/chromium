// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_EXECUTION_ENGINE_DISPATCHER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_EXECUTION_ENGINE_DISPATCHER_H_

#include <memory>
#include <variant>

#include "chrome/browser/actor/tools/tool_result.h"

namespace actor {

class ToolDelegate;
class ToolRequest;

// Supplies ToolRequests for ExecutionEngine to execute.
class ExecutionEngineDispatcher {
 public:
  using NextAction =
      std::variant<std::unique_ptr<ToolRequest>, ToolResultWithLatencyInfo>;

  virtual ~ExecutionEngineDispatcher() = default;

  // Returns the next ToolRequest to execute. Once the dispatcher has run out
  // of requests (because it ran to completion, or a failure occurred) it must
  // return a ToolResultWithLatencyInfo to signify its end. Once the
  // ToolResultWithLatencyInfo is returned the caller must not call GetNext
  // again.
  virtual NextAction GetNext(ToolDelegate& tool_delegate) = 0;

  // Called when a ToolRequest from GetNext has been run to completion to
  // return the result to the originating dispatcher.
  virtual void OnToolCompleted(ToolResultWithLatencyInfo result) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_EXECUTION_ENGINE_DISPATCHER_H_
