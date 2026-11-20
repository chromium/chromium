// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_screenshot_tool.h"

#include <memory>
#include <string>
#include <utility>

#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "url/gurl.h"

namespace actor {

GetScreenshotTool::GetScreenshotTool(TaskId task_id,
                                     ToolDelegate& tool_delegate,
                                     ActorSurface& actor_surface)
    : Tool(task_id, tool_delegate) {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
}

GetScreenshotTool::~GetScreenshotTool() = default;

void GetScreenshotTool::Validate(DeprecatedToolCallback callback) {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
  PostResponseTask(std::move(callback),
                   MakeResult(mojom::ActionResultCode::kToolUnknown));
}

mojom::ActionResultPtr GetScreenshotTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
  return MakeResult(mojom::ActionResultCode::kToolUnknown);
}

void GetScreenshotTool::Invoke(ToolResultCallback callback) {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
  PostResponseTask(std::move(callback),
                   ToolResult::Error(ToolError(
                       mojom::ActionResultCode::kScreenshotCaptureFailed)));
}

std::string GetScreenshotTool::DebugString() const {
  return "GetScreenshotTool";
}

std::string GetScreenshotTool::JournalEvent() const {
  return "GetScreenshot";
}

GURL GetScreenshotTool::JournalURL() const {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
  return GURL();
}

std::unique_ptr<ObservationDelayController>
GetScreenshotTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

ActorSurfaceHandle GetScreenshotTool::GetTargetActorSurface() const {
  // TODO(crbug.com/573253710): Implement.
  NOTIMPLEMENTED();
  return ActorSurfaceHandle();
}

}  // namespace actor
