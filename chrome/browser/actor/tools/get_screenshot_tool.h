// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"

class GURL;

namespace actor {

class ActorSurface;

// Captures a screenshot of the active viewport of a tab.
class GetScreenshotTool : public Tool {
 public:
  GetScreenshotTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    ActorSurface& actor_surface);
  ~GetScreenshotTool() override;

  // Tool:
  void Validate(DeprecatedToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolResultCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  GURL JournalURL() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_
