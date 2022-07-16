// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace extensions {
namespace {

// Error messages.
const char kInvalidTaskType[] = "Invalid task type: ";
const char kInvalidFileUrl[] = "Invalid file URL";

// Make a set of unique filename suffixes out of the list of file URLs.
std::set<std::string> GetUniqueSuffixes(
    const std::vector<std::string>& url_list,
    const storage::FileSystemContext* context) {
  std::set<std::string> suffixes;
  for (size_t i = 0; i < url_list.size(); ++i) {
    const FileSystemURL url =
        context->CrackURLInFirstPartyContext(GURL(url_list[i]));
    if (!url.is_valid() || url.path().empty())
      return std::set<std::string>();
    // We'll skip empty suffixes.
    if (!url.path().Extension().empty())
      suffixes.insert(url.path().Extension());
  }
  return suffixes;
}

// Make a set of unique MIME types out of the list of MIME types.
std::set<std::string> GetUniqueMimeTypes(
    const std::vector<std::string>& mime_type_list) {
  std::set<std::string> mime_types;
  for (size_t i = 0; i < mime_type_list.size(); ++i) {
    const std::string mime_type = mime_type_list[i];
    // We'll skip empty MIME types and existing MIME types.
    if (!mime_type.empty())
      mime_types.insert(mime_type);
  }
  return mime_types;
}

}  // namespace

FileManagerPrivateInternalExecuteTaskFunction::
    FileManagerPrivateInternalExecuteTaskFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteTaskFunction::Run() {
  using extensions::api::file_manager_private_internal::ExecuteTask::Params;
  using extensions::api::file_manager_private_internal::ExecuteTask::Results::
      Create;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::file_tasks::TaskType task_type =
      file_manager::file_tasks::StringToTaskType(params->descriptor.task_type);
  if (task_type == file_manager::file_tasks::TASK_TYPE_UNKNOWN) {
    return RespondNow(Error(kInvalidTaskType + params->descriptor.task_type));
  }
  file_manager::file_tasks::TaskDescriptor task(
      params->descriptor.app_id, task_type, params->descriptor.action_id);

  if (params->urls.empty()) {
    return RespondNow(ArgumentList(
        Create(extensions::api::file_manager_private::TASK_RESULT_EMPTY)));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  std::vector<FileSystemURL> urls;
  for (size_t i = 0; i < params->urls.size(); i++) {
    const FileSystemURL url =
        file_system_context->CrackURLInFirstPartyContext(GURL(params->urls[i]));
    if (!chromeos::FileSystemBackend::CanHandleURL(url)) {
      return RespondNow(Error(kInvalidFileUrl));
    }
    urls.push_back(url);
  }

  const bool result = file_manager::file_tasks::ExecuteFileTask(
      profile, task, urls,
      base::BindOnce(
          &FileManagerPrivateInternalExecuteTaskFunction::OnTaskExecuted,
          this));
  if (!result) {
    return RespondNow(Error("ExecuteFileTask failed"));
  }
  return RespondLater();
}

void FileManagerPrivateInternalExecuteTaskFunction::OnTaskExecuted(
    extensions::api::file_manager_private::TaskResult result,
    std::string failure_reason) {
  auto result_list = extensions::api::file_manager_private_internal::
      ExecuteTask::Results::Create(result);
  if (result == extensions::api::file_manager_private::TASK_RESULT_FAILED) {
    Respond(Error("Task result failed: " + failure_reason));
  } else {
    Respond(ArgumentList(std::move(result_list)));
  }
}

FileManagerPrivateInternalGetFileTasksFunction::
    FileManagerPrivateInternalGetFileTasksFunction() = default;

FileManagerPrivateInternalGetFileTasksFunction::
    ~FileManagerPrivateInternalGetFileTasksFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetFileTasksFunction::Run() {
  using extensions::api::file_manager_private_internal::GetFileTasks::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->urls.empty())
    return RespondNow(Error("No URLs provided"));

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (size_t i = 0; i < params->urls.size(); ++i) {
    const GURL url(params->urls[i]);
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(url));
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    urls_.push_back(url);
    local_paths_.push_back(file_system_url.path());
  }

  mime_type_collector_ =
      std::make_unique<app_file_handler_util::MimeTypeCollector>(profile);
  mime_type_collector_->CollectForLocalPaths(
      local_paths_,
      base::BindOnce(
          &FileManagerPrivateInternalGetFileTasksFunction::OnMimeTypesCollected,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalGetFileTasksFunction::OnMimeTypesCollected(
    std::unique_ptr<std::vector<std::string>> mime_types) {
  is_directory_collector_ =
      std::make_unique<app_file_handler_util::IsDirectoryCollector>(
          Profile::FromBrowserContext(browser_context()));
  is_directory_collector_->CollectForEntriesPaths(
      local_paths_,
      base::BindOnce(&FileManagerPrivateInternalGetFileTasksFunction::
                         OnAreDirectoriesAndMimeTypesCollected,
                     this, std::move(mime_types)));
}

void FileManagerPrivateInternalGetFileTasksFunction::
    OnAreDirectoriesAndMimeTypesCollected(
        std::unique_ptr<std::vector<std::string>> mime_types,
        std::unique_ptr<std::set<base::FilePath>> directory_paths) {
  std::vector<EntryInfo> entries;
  for (size_t i = 0; i < local_paths_.size(); ++i) {
    entries.emplace_back(
        local_paths_[i], (*mime_types)[i],
        directory_paths->find(local_paths_[i]) != directory_paths->end());
  }

  file_manager::file_tasks::FindAllTypesOfTasks(
      Profile::FromBrowserContext(browser_context()), entries, urls_,
      base::BindOnce(
          &FileManagerPrivateInternalGetFileTasksFunction::OnFileTasksListed,
          this));
}

void FileManagerPrivateInternalGetFileTasksFunction::OnFileTasksListed(
    std::unique_ptr<std::vector<file_manager::file_tasks::FullTaskDescriptor>>
        tasks) {
  // Convert the tasks into JSON compatible objects.
  using api::file_manager_private::FileTask;
  std::vector<FileTask> results;
  for (const file_manager::file_tasks::FullTaskDescriptor& task : *tasks) {
    FileTask converted;
    converted.descriptor.app_id = task.task_descriptor.app_id;
    converted.descriptor.task_type =
        TaskTypeToString(task.task_descriptor.task_type);
    converted.descriptor.action_id = task.task_descriptor.action_id;
    if (!task.icon_url.is_empty())
      converted.icon_url = task.icon_url.spec();
    converted.title = task.task_title;
    converted.verb = task.task_verb;
    converted.is_default = task.is_default;
    converted.is_generic_file_handler = task.is_generic_file_handler;
    results.push_back(std::move(converted));
  }

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           GetFileTasks::Results::Create(results)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSetDefaultTaskFunction::Run() {
  using extensions::api::file_manager_private_internal::SetDefaultTask::Params;
  const std::unique_ptr<Params> params(Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  const std::set<std::string> suffixes =
      GetUniqueSuffixes(params->urls, file_system_context.get());
  const std::set<std::string> mime_types =
      GetUniqueMimeTypes(params->mime_types);

  // If there weren't any mime_types, and all the suffixes were blank,
  // then we "succeed", but don't actually associate with anything.
  // Otherwise, any time we set the default on a file with no extension
  // on the local drive, we'd fail.
  // TODO(gspencer): Fix file manager so that it never tries to set default in
  // cases where extensionless local files are part of the selection.
  if (suffixes.empty() && mime_types.empty()) {
    return RespondNow(OneArgument(base::Value(true)));
  }

  file_manager::file_tasks::TaskType task_type =
      file_manager::file_tasks::StringToTaskType(params->descriptor.task_type);
  if (task_type == file_manager::file_tasks::TASK_TYPE_UNKNOWN) {
    return RespondNow(Error(kInvalidTaskType + params->descriptor.task_type));
  }
  file_manager::file_tasks::TaskDescriptor descriptor(
      params->descriptor.app_id, task_type, params->descriptor.action_id);

  file_manager::file_tasks::UpdateDefaultTask(profile->GetPrefs(), descriptor,
                                              suffixes, mime_types);
  return RespondNow(NoArguments());
}

}  // namespace extensions
