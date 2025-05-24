// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/common/actor.mojom-forward.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

class Tool;

// Entry point into actor tool usage. ToolController is a profile-scoped,
// ActorCoordinator-owned object. This class routes a tool use request to the
// appropriate browser tool or to a corresponding executor in the renderer for
// page-level tools.
class ToolController {
 public:
  ToolController();
  ~ToolController();
  ToolController(const ToolController&) = delete;
  ToolController& operator=(const ToolController&) = delete;

  // Invokes a tool action.
  void Invoke(const ToolInvocation& action,
              ToolInvocation::ResultCallback result_callback);

  // Call to clear the current tool invocation and return the given result to
  // the initiator. Must only be called when a tool invocation is in-progress.
  void CompleteToolRequest(mojom::ActionResultPtr result);

 private:
  std::unique_ptr<Tool> CreateTool(content::RenderFrameHost& frame,
                                   const ToolInvocation& invocation);

  void ValidationComplete(mojom::ActionResultPtr result);

  // This state is non-null whenever a tool invocation is in progress.
  struct ActiveState {
    ActiveState(std::unique_ptr<Tool> tool,
                ToolInvocation::ResultCallback completion_callback);
    ~ActiveState();
    ActiveState(const ActiveState&) = delete;
    ActiveState& operator=(const ActiveState&) = delete;

    // Both `tool` and `completion_callback` are guaranteed to be non-null while
    // active_state_ is set.
    std::unique_ptr<Tool> tool;
    ToolInvocation::ResultCallback completion_callback;
  };
  std::optional<ActiveState> active_state_;

  base::WeakPtrFactory<ToolController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_CONTROLLER_H_
