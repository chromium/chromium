// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "components/actor/core/shared_types.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

class ActorSurface;
class ActorTask;

// Tool responsible for intercepting file chooser dialogs and injecting
// authorized files into page file inputs during actor task execution.
//
// Invoking the tool:
//   1. downloads each source URL into memory. Redirects are not followed and
//      fail the download. Only URLs on the hosts listed in
//      kGlicActorFileUploadToolCredentialedHosts are fetched with the user's
//      credentials,
//   2. registers the contents with storage::InMemoryFileRegistry, which holds
//      them in browser process memory,
//   3. installs FilePickerInterceptor for the frame holding the target, so its
//      next file chooser request is answered with those files rather than a
//      native file picker, and
//   4. enqueues a click on the target element, which is what causes the page to
//      open the file chooser.
class FileUploadTool : public Tool {
 public:
  FileUploadTool(TaskId task_id,
                 ToolDelegate& tool_delegate,
                 ActorSurface& actor_surface,
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
  GURL JournalURL() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  // A file whose contents are being collected before being staged in memory.
  struct PendingFile {
    PendingFile();
    PendingFile(PendingFile&&);
    PendingFile& operator=(PendingFile&&);
    ~PendingFile();

    // UTF-8.
    std::string display_name;
    std::string contents;
  };

  void StartDownloads();
  void OnDownloadComplete(size_t index, std::optional<std::string> body);
  void OnAllDownloadsComplete();

  // Registers `files` in memory, installs the file picker interceptor for the
  // frame holding the target and enqueues the click that makes the page open
  // its file chooser.
  void StageFilesAndClick(std::vector<PendingFile> files);

  // Completes the invocation with a failure, logging `message` to the journal.
  void FailInvoke(mojom::ActionResultCode code, std::string_view message);

  ActorSurfaceHandle actor_surface_handle_;
  PageTarget target_;
  std::vector<FileUploadSource> files_;

  ToolCallback invoke_callback_;

  // Download state. `downloaded_contents_` is indexed in parallel with
  // `files_`.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::vector<std::optional<std::string>> downloaded_contents_;
  size_t pending_downloads_ = 0;
  bool download_failed_ = false;

  base::WeakPtrFactory<FileUploadTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FILE_UPLOAD_TOOL_H_
