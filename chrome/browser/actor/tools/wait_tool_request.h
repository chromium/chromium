// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {
class ToolRequestVisitorFunctor;

class WaitToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "Wait";

  explicit WaitToolRequest(base::TimeDelta wait_duration,
                           ActorSurfaceHandle observe_actor_surface_handle =
                               ActorSurfaceHandle::Null());
  ~WaitToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool RequiresUrlCheckInCurrentTab() const override;
  ActorSurfaceHandle GetActorSurfaceForValidation() const override;

 private:
  base::TimeDelta wait_duration_;

  // Optional. If provided, an observation from this actor surface will be
  // included in the result observations. However, this actor surface is not
  // added to the actor surface set in ActorTask.
  ActorSurfaceHandle observe_actor_surface_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
