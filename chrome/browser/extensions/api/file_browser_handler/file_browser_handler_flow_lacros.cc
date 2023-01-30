// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_browser_handler/file_browser_handler_flow_lacros.h"

#include <memory>
#include <set>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"

namespace extensions {

namespace {

using extensions::Extension;
using extensions::GrantedFileEntry;

using extensions::app_file_handler_util::CreateEntryInfos;
using extensions::app_file_handler_util::CreateFileEntry;
using extensions::app_file_handler_util::PrepareFilesForWritableApp;

// This class prepares file entries for fileBrowserHandler, and dispatches the
// data to JavaScript code of the specified |extension|..
class FileBrowserHandlerExecutorFlow {
 public:
  // |done| is called with final results after Execute() runs.
  FileBrowserHandlerExecutorFlow(FileBrowserHandlerFlowFinishedCallback done,
                                 Profile* profile,
                                 const Extension* extension,
                                 const std::string& action_id,
                                 std::vector<base::FilePath>&& entry_paths,
                                 std::vector<std::string>&& mime_types);

  FileBrowserHandlerExecutorFlow(const FileBrowserHandlerExecutorFlow&) =
      delete;
  FileBrowserHandlerExecutorFlow& operator=(
      const FileBrowserHandlerExecutorFlow&) = delete;

  // Main entry point to start the execution flow.
  void Execute();

 private:
  // This object is responsible for deleting itself.
  virtual ~FileBrowserHandlerExecutorFlow();

  void OnAreDirectoriesCollected(
      std::unique_ptr<std::set<base::FilePath>> directory_paths);
  void OnFilesValid(std::unique_ptr<std::set<base::FilePath>> directory_paths);
  void OnFilesInvalid(const base::FilePath& /*error_path*/);
  void PrepareToLaunch();
  void GrantAccessToFilesAndLaunch(
      std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo>
          context_info);

  // All flows must converge here, which deallocates the instance.
  void Finish(bool success);

  FileBrowserHandlerFlowFinishedCallback done_;

  raw_ptr<Profile> profile_;
  scoped_refptr<const Extension> extension_;

  // Inputs owned by the class.
  const std::string action_id_;
  const std::vector<base::FilePath> entry_paths_;
  const std::vector<std::string> mime_types_;

  extensions::app_file_handler_util::IsDirectoryCollector
      is_directory_collector_;

  std::vector<extensions::EntryInfo> entries_;
};

FileBrowserHandlerExecutorFlow::FileBrowserHandlerExecutorFlow(
    FileBrowserHandlerFlowFinishedCallback done,
    Profile* profile,
    const Extension* extension,
    const std::string& action_id,
    std::vector<base::FilePath>&& entry_paths,
    std::vector<std::string>&& mime_types)
    : done_(std::move(done)),
      profile_(profile),
      extension_(extension),
      action_id_(action_id),      // Copies.
      entry_paths_(entry_paths),  // Takes ownership.
      mime_types_(mime_types),    // Takes ownership.
      is_directory_collector_(profile) {}

FileBrowserHandlerExecutorFlow::~FileBrowserHandlerExecutorFlow() = default;

void FileBrowserHandlerExecutorFlow::Execute() {
  // Forbid calling undeclared handlers.
  if (!FileBrowserHandler::FindForActionId(extension_.get(), action_id_)) {
    Finish(false);  // Action ID not found.
    return;
  }

  // This imposes the assumption that ExecuteFileBrowserHandlerFlow() is invoked
  // by a nontrivial fileBrowserHandler request. Hereafter, if the provided
  // files turn out to be invalid (e.g., due to permission or race conditions
  // involving file changes), it's still possible for the extension to execute
  // with an empty file list.
  if (entry_paths_.empty()) {
    Finish(false);  // File list empty.
    return;
  }

#if DCHECK_IS_ON()
  for (const auto& entry_path : entry_paths_) {
    DCHECK(entry_path.IsAbsolute());
  }
#endif  // DCHECK_IS_ON()

  is_directory_collector_.CollectForEntriesPaths(
      entry_paths_,
      base::BindOnce(&FileBrowserHandlerExecutorFlow::OnAreDirectoriesCollected,
                     base::Unretained(this)));
}

void FileBrowserHandlerExecutorFlow::OnAreDirectoriesCollected(
    std::unique_ptr<std::set<base::FilePath>> directory_paths) {
  const FileBrowserHandler* action =
      FileBrowserHandler::FindForActionId(extension_.get(), action_id_);
  if (action->CanWrite()) {
    std::set<base::FilePath>* const directory_paths_ptr = directory_paths.get();
    PrepareFilesForWritableApp(
        entry_paths_, profile_, *directory_paths_ptr,
        base::BindOnce(&FileBrowserHandlerExecutorFlow::OnFilesValid,
                       base::Unretained(this), std::move(directory_paths)),
        base::BindOnce(&FileBrowserHandlerExecutorFlow::OnFilesInvalid,
                       base::Unretained(this)));
  } else {
    OnFilesValid(std::move(directory_paths));
  }
}

void FileBrowserHandlerExecutorFlow::OnFilesValid(
    std::unique_ptr<std::set<base::FilePath>> directory_paths) {
  entries_ = CreateEntryInfos(entry_paths_, mime_types_, *directory_paths);
  // |entry_paths_| and |mime_types_| should not be used from this point on.
  PrepareToLaunch();
}

void FileBrowserHandlerExecutorFlow::OnFilesInvalid(
    const base::FilePath& /*error_path*/) {
  DCHECK(entries_.empty());
  // |entry_paths_| and |mime_types_| should not be used from this point on.
  PrepareToLaunch();
}

void FileBrowserHandlerExecutorFlow::PrepareToLaunch() {
  // Access needs to be granted to the file for the process associated with
  // the extension. To do this the ExtensionHost is needed. This might not be
  // available, or it might be in the process of being unloaded, in which case
  // the lazy background task queue is used to load the extension and then
  // call back to us.
  const extensions::LazyContextId context_id(profile_, extension_->id());
  extensions::LazyContextTaskQueue* task_queue = context_id.GetTaskQueue();

  if (task_queue->ShouldEnqueueTask(profile_, extension_.get())) {
    task_queue->AddPendingTask(
        context_id,
        base::BindOnce(
            &FileBrowserHandlerExecutorFlow::GrantAccessToFilesAndLaunch,
            base::Unretained(this)));
    return;
  }

  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(profile_);
  extensions::ExtensionHost* extension_host =
      manager->GetBackgroundHostForExtension(extension_->id());

  if (extension_host) {
    GrantAccessToFilesAndLaunch(
        std::make_unique<extensions::LazyContextTaskQueue::ContextInfo>(
            extension_host));
  } else {
    Finish(false);  // No background page available.
  }
}

void FileBrowserHandlerExecutorFlow::GrantAccessToFilesAndLaunch(
    std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo>
        context_info) {
  if (!context_info) {
    Finish(false);  // Failed to start app.
    return;
  }

  int handler_pid = context_info->render_process_host->GetID();
  if (handler_pid <= 0) {
    Finish(false);  // No app available.
    return;
  }

  extensions::EventRouter* router = extensions::EventRouter::Get(profile_);
  if (!router) {
    Finish(false);  // Could not send task to app.
    return;
  }

  base::Value::List event_args;
  event_args.Append(action_id_);
  base::Value::Dict details;
  // Creates file_entries, which will be converted into FileEntry or
  // DirectoryEntry instances by fileBrowserHandler JS massager.
  base::Value::List file_entries;
  file_entries.reserve(entries_.size());
  for (const auto& entry : entries_) {
    GrantedFileEntry granted_file_entry = CreateFileEntry(
        profile_, extension_.get(), context_info->render_process_host->GetID(),
        entry.path, entry.is_directory);
    base::Value::Dict item;
    item.Set("fileSystemId", granted_file_entry.filesystem_id);
    item.Set("baseName", granted_file_entry.registered_name);
    item.Set("mimeType", entry.mime_type);
    item.Set("entryId", granted_file_entry.id);
    item.Set("isDirectory", entry.is_directory);
    file_entries.Append(std::move(item));
  }
  details.Set("entries", std::move(file_entries));
  event_args.Append(std::move(details));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::FILE_BROWSER_HANDLER_ON_EXECUTE,
      "fileBrowserHandler.onExecute", std::move(event_args), profile_);
  router->DispatchEventToExtension(extension_->id(), std::move(event));

  Finish(true);  // Success.
}

void FileBrowserHandlerExecutorFlow::Finish(bool success) {
  if (done_)
    std::move(done_).Run(success);

  delete this;
}

}  // namespace

void ExecuteFileBrowserHandlerFlow(
    Profile* profile,
    const Extension* extension,
    const std::string& action_id,
    std::vector<base::FilePath>&& entry_paths,
    std::vector<std::string>&& mime_types,
    FileBrowserHandlerFlowFinishedCallback done) {
  DCHECK_EQ(entry_paths.size(), mime_types.size());

  // The executor object will self-delete on completion.
  (new FileBrowserHandlerExecutorFlow(std::move(done), profile, extension,
                                      action_id, std::move(entry_paths),
                                      std::move(mime_types)))
      ->Execute();
}

}  // namespace extensions
