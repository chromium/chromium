// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_browser_handlers.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_util.h"

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionList;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;
using storage::FileSystemURL;

namespace file_manager::file_browser_handlers {

namespace {

// This class is used to execute a file browser handler task. Here's how this
// works:
//
// 1) Open the "external" file system
// 2) Set up permissions for the target files on the external file system.
// 3) Raise onExecute event with the action ID and entries of the target
//    files. The event will launch the file browser handler if not active.
// 4) In the file browser handler, onExecute event is handled and executes the
//    task in JavaScript.
//
// That said, the class itself does not execute a task. The task will be
// executed in JavaScript.
class FileBrowserHandlerExecutor {
 public:
  FileBrowserHandlerExecutor(Profile* profile,
                             const Extension* extension,
                             const std::string& action_id);

  FileBrowserHandlerExecutor(const FileBrowserHandlerExecutor&) = delete;
  FileBrowserHandlerExecutor& operator=(const FileBrowserHandlerExecutor&) =
      delete;

  // Executes the task for each file. |done| will be run with the result.
  void Execute(const std::vector<FileSystemURL>& file_urls,
               file_tasks::FileTaskFinishedCallback done);

 private:
  // This object is responsible to delete itself.
  virtual ~FileBrowserHandlerExecutor();

  // Checks legitimacy of file url and grants file RO access permissions from
  // handler (target) extension and its renderer process.
  static std::unique_ptr<FileDefinitionList> SetupFileAccessPermissions(
      scoped_refptr<storage::FileSystemContext> file_system_context_handler,
      const scoped_refptr<const Extension>& handler_extension,
      const std::vector<FileSystemURL>& file_urls);

  void ExecuteDoneOnUIThread(bool success, std::string failure_reason);
  void ExecuteAfterSetupFileAccess(
      std::unique_ptr<FileDefinitionList> file_list);
  void ExecuteFileActionsOnUIThread(
      std::unique_ptr<FileDefinitionList> file_definition_list,
      std::unique_ptr<EntryDefinitionList> entry_definition_list);
  void SetupPermissionsAndDispatchEvent(
      std::unique_ptr<FileDefinitionList> file_definition_list,
      std::unique_ptr<EntryDefinitionList> entry_definition_list,
      std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo>
          context_info);

  // Registers file permissions from |handler_host_permissions_| with
  // ChildProcessSecurityPolicy for process with id |handler_pid|.
  void SetupHandlerHostFileAccessPermissions(
      FileDefinitionList* file_definition_list,
      const Extension* extension,
      int handler_pid);

  raw_ptr<Profile> profile_;
  scoped_refptr<const Extension> extension_;
  const std::string action_id_;
  file_tasks::FileTaskFinishedCallback done_;
  base::WeakPtrFactory<FileBrowserHandlerExecutor> weak_ptr_factory_{this};
};

// static
std::unique_ptr<FileDefinitionList>
FileBrowserHandlerExecutor::SetupFileAccessPermissions(
    scoped_refptr<storage::FileSystemContext> file_system_context_handler,
    const scoped_refptr<const Extension>& handler_extension,
    const std::vector<FileSystemURL>& file_urls) {
  DCHECK(handler_extension.get());

  auto* backend = ash::FileSystemBackend::Get(*file_system_context_handler);

  std::unique_ptr<FileDefinitionList> file_definition_list(
      new FileDefinitionList);
  for (size_t i = 0; i < file_urls.size(); ++i) {
    const FileSystemURL& url = file_urls[i];

    // Check if this file system entry exists first.
    base::File::Info file_info;

    base::FilePath local_path = url.path();
    base::FilePath virtual_path = url.virtual_path();

    const bool is_native_file = url.type() == storage::kFileSystemTypeLocal;

    // If the file is from a physical volume, actual file must be found.
    if (is_native_file) {
      if (!base::PathExists(local_path) || base::IsLink(local_path) ||
          !base::GetFileInfo(local_path, &file_info)) {
        continue;
      }
    }

    // Grant access to this particular file to target extension. This will
    // ensure that the target extension can access only this FS entry and
    // prevent from traversing FS hierarchy upward.
    backend->GrantFileAccessToOrigin(handler_extension->origin(), virtual_path);

    // Output values.
    FileDefinition file_definition;
    file_definition.virtual_path = virtual_path;
    file_definition.is_directory = file_info.is_directory;
    file_definition.absolute_path = local_path;
    file_definition_list->push_back(file_definition);
  }

  return file_definition_list;
}

FileBrowserHandlerExecutor::FileBrowserHandlerExecutor(
    Profile* profile,
    const Extension* extension,
    const std::string& action_id)
    : profile_(profile), extension_(extension), action_id_(action_id) {}

FileBrowserHandlerExecutor::~FileBrowserHandlerExecutor() = default;

void FileBrowserHandlerExecutor::Execute(
    const std::vector<FileSystemURL>& file_urls,
    file_tasks::FileTaskFinishedCallback done) {
  done_ = std::move(done);

  // Get file system context for the extension to which onExecute event will be
  // sent. The file access permissions will be granted to the extension in the
  // file system context for the files in |file_urls|.
  scoped_refptr<storage::FileSystemContext> file_system_context(
      util::GetFileSystemContextForSourceURL(profile_, extension_->url()));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&SetupFileAccessPermissions, file_system_context,
                     extension_, file_urls),
      base::BindOnce(&FileBrowserHandlerExecutor::ExecuteAfterSetupFileAccess,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FileBrowserHandlerExecutor::ExecuteAfterSetupFileAccess(
    std::unique_ptr<FileDefinitionList> file_definition_list) {
  // Outlives the conversion process, since bound to the callback.
  const FileDefinitionList& file_definition_list_ref =
      *file_definition_list.get();
  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile_,
                                                           extension_->url()),
      url::Origin::Create(extension_->url()), file_definition_list_ref,
      base::BindOnce(&FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(file_definition_list)));
}

void FileBrowserHandlerExecutor::ExecuteDoneOnUIThread(
    bool success,
    std::string failure_reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (done_) {
    // In a multiprofile session, extension handlers will open on the desktop
    // corresponding to the profile that owns the files, so return
    // TASK_RESULT_MESSAGE_SENT.
    std::move(done_).Run(
        success
            ? extensions::api::file_manager_private::TaskResult::kMessageSent
            : extensions::api::file_manager_private::TaskResult::kFailed,
        failure_reason);
  }
  delete this;
}

void FileBrowserHandlerExecutor::ExecuteFileActionsOnUIThread(
    std::unique_ptr<FileDefinitionList> file_definition_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (file_definition_list->empty() || entry_definition_list->empty()) {
    ExecuteDoneOnUIThread(false, "File list empty");
    return;
  }

  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(profile_);
  extensions::ExtensionHost* extension_host =
      manager->GetBackgroundHostForExtension(extension_->id());

  const auto context_id =
      extensions::LazyContextId::ForExtension(profile_, extension_.get());
  CHECK(context_id.IsForBackgroundPage());
  extensions::LazyContextTaskQueue* task_queue = context_id.GetTaskQueue();

  if (task_queue->ShouldEnqueueTask(profile_, extension_.get())) {
    task_queue->AddPendingTask(
        context_id,
        base::BindOnce(
            &FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent,
            weak_ptr_factory_.GetWeakPtr(), std::move(file_definition_list),
            std::move(entry_definition_list)));
  } else if (extension_host) {
    SetupPermissionsAndDispatchEvent(
        std::move(file_definition_list), std::move(entry_definition_list),
        std::make_unique<extensions::LazyContextTaskQueue::ContextInfo>(
            extension_host));
  } else {
    ExecuteDoneOnUIThread(false, "No background page available");
  }
}

void FileBrowserHandlerExecutor::SetupPermissionsAndDispatchEvent(
    std::unique_ptr<FileDefinitionList> file_definition_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list,
    std::unique_ptr<extensions::LazyContextTaskQueue::ContextInfo>
        context_info) {
  if (!context_info) {
    ExecuteDoneOnUIThread(false, "Failed to start app");
    return;
  }

  int handler_pid = context_info->render_process_host->GetID();
  if (handler_pid <= 0) {
    ExecuteDoneOnUIThread(false, "No app available");
    return;
  }

  extensions::EventRouter* router = extensions::EventRouter::Get(profile_);
  if (!router) {
    ExecuteDoneOnUIThread(false, "Could not send task to app");
    return;
  }

  SetupHandlerHostFileAccessPermissions(file_definition_list.get(),
                                        extension_.get(), handler_pid);

  base::Value::List event_args;
  event_args.Append(action_id_);
  base::Value::Dict details;
  // Get file definitions. These will be replaced with Entry instances by
  // dispatchEvent() method from event_binding.js.
  auto file_entries = file_manager::util::ConvertEntryDefinitionListToListValue(
      *entry_definition_list);

  details.Set("entries", std::move(file_entries));
  event_args.Append(std::move(details));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::FILE_BROWSER_HANDLER_ON_EXECUTE,
      "fileBrowserHandler.onExecute", std::move(event_args), profile_);
  router->DispatchEventToExtension(extension_->id(), std::move(event));

  ExecuteDoneOnUIThread(true, "");
}

void FileBrowserHandlerExecutor::SetupHandlerHostFileAccessPermissions(
    FileDefinitionList* file_definition_list,
    const Extension* extension,
    int handler_pid) {
  const FileBrowserHandler* action =
      FileBrowserHandler::FindForActionId(extension_.get(), action_id_);
  for (FileDefinitionList::const_iterator iter = file_definition_list->begin();
       iter != file_definition_list->end(); ++iter) {
    if (!action) {
      continue;
    }
    if (action->CanRead()) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(
          handler_pid, iter->absolute_path);
    }
    if (action->CanWrite()) {
      content::ChildProcessSecurityPolicy::GetInstance()
          ->GrantCreateReadWriteFile(handler_pid, iter->absolute_path);
    }
  }
}

}  // namespace

bool ExecuteFileBrowserHandler(Profile* profile,
                               const Extension* extension,
                               const std::string& action_id,
                               const std::vector<FileSystemURL>& file_urls,
                               file_tasks::FileTaskFinishedCallback done) {
  // Forbid calling undeclared handlers.
  if (!FileBrowserHandler::FindForActionId(extension, action_id)) {
    return false;
  }

  // The executor object will be self deleted on completion.
  (new FileBrowserHandlerExecutor(profile, extension, action_id))
      ->Execute(file_urls, std::move(done));
  return true;
}

}  // namespace file_manager::file_browser_handlers
