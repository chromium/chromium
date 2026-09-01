// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

FileUploadTool::FileUploadTool(TaskId task_id,
                               ToolDelegate& tool_delegate,
                               tabs::TabInterface& tab,
                               PageTarget target,
                               std::vector<FileUploadSource> files)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      target_(std::move(target)),
      files_(std::move(files)) {}

FileUploadTool::~FileUploadTool() = default;

void FileUploadTool::Validate(ToolCallback callback) {
  // Skeleton: validation will check ActorTask allowed files session registry.
  std::move(callback).Run(MakeOkResult(/*requires_page_stabilization=*/false));
}

mojom::ActionResultPtr FileUploadTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  // Skeleton: TOCTOU checks on the target element.
  return MakeOkResult(/*requires_page_stabilization=*/false);
}

void FileUploadTool::Invoke(ToolCallback callback) {
  // Skeleton: Setup FilePickerInterceptor, trigger click on target, and
  // handle file injection.
  std::move(callback).Run(MakeOkResult(/*requires_page_stabilization=*/true));
}

void FileUploadTool::Cancel() {
  // Skeleton: Clean up any interceptor or pending state.
}

std::string FileUploadTool::DebugString() const {
  return base::StringPrintf("FileUploadTool(files_count=%zu)", files_.size());
}

std::string FileUploadTool::JournalEvent() const {
  return "FileUpload";
}

std::unique_ptr<ObservationDelayController>
FileUploadTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  tabs::TabInterface* const tab = tab_handle_.Get();
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }
  content::RenderFrameHost* const rfh =
      tab->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

tabs::TabHandle FileUploadTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
