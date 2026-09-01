// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

// Tool responsible for intercepting file chooser dialogs and injecting
// authorized files into page file inputs during actor task execution.
class FileUploadTool : public Tool {
 public:
  FileUploadTool(TaskId task_id,
                 ToolDelegate& tool_delegate,
                 tabs::TabInterface& tab,
                 PageTarget target,
                 std::vector<FileUploadSource> files);
  ~FileUploadTool() override;

  // Tool:
  void Validate(ToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  tabs::TabHandle tab_handle_;
  PageTarget target_;
  std::vector<FileUploadSource> files_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_
