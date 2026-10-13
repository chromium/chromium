// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"

namespace actor {

// Waits for a page to settle before continuing with other tools.
class WaitTool : public Tool {
 public:
  explicit WaitTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    base::TimeDelta wait_duration,
                    ActorSurfaceHandle observe_actor_surface_handle);
  ~WaitTool() override;

  // actor::Tool
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

  static void SetNoDelayForTesting();

 private:
  void OnDelayFinished(ToolCallback callback);

  // TODO(bokan): This could be removed in place of tests setting the wait
  // duration explicitly.
  static bool no_delay_for_testing_;

  base::TimeDelta wait_duration_;

  // The surface to observe after waiting, or a null handle if none.
  ActorSurfaceHandle observe_actor_surface_handle_;

  base::WeakPtrFactory<WaitTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_H_
