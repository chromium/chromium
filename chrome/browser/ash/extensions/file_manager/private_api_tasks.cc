// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_tasks.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace extensions {
namespace {

// Error messages.
constexpr char kInvalidTaskType[] = "Invalid task type: ";
constexpr char kInvalidFileUrl[] = "Invalid file URL";

// Make a set of unique filename suffixes out of the list of file URLs.
std::set<std::string> GetUniqueSuffixes(
    const std::vector<std::string>& file_urls,
    const storage::FileSystemContext* context) {
  std::set<std::string> suffixes;
  for (const auto& file_url : file_urls) {
    const storage::FileSystemURL url =
        context->CrackURLInFirstPartyContext(GURL{file_url});
    if (!url.is_valid() || url.path().empty()) {
      return {};
    }
    // We'll skip empty suffixes.
    if (!url.path().Extension().empty()) {
      suffixes.insert(url.path().Extension());
    }
  }
  return suffixes;
}

// Make a set of unique MIME types out of the list of MIME types.
std::set<std::string> GetUniqueMimeTypes(
    const std::vector<std::string>& mime_type_list) {
  std::set<std::string> mime_types;
  for (const auto& mime_type : mime_type_list) {
    // We'll skip empty MIME types and existing MIME types.
    if (!mime_type.empty()) {
      mime_types.insert(mime_type);
    }
  }
  return mime_types;
}

namespace api_fmp = extensions::api::file_manager_private;
namespace api_fmp_internal = extensions::api::file_manager_private_internal;

std::optional<api_fmp::PolicyDefaultHandlerStatus>
RemapPolicyDefaultHandlerStatus(
    const std::optional<file_manager::file_tasks::PolicyDefaultHandlerStatus>&
        status) {
  if (!status) {
    return {};
  }

  switch (*status) {
    case file_manager::file_tasks::PolicyDefaultHandlerStatus::
        kDefaultHandlerAssignedByPolicy:
      return api_fmp::PolicyDefaultHandlerStatus::
          kDefaultHandlerAssignedByPolicy;
    case file_manager::file_tasks::PolicyDefaultHandlerStatus::
        kIncorrectAssignment:
      return api_fmp::PolicyDefaultHandlerStatus::kIncorrectAssignment;
  }
}

}  // namespace

FileManagerPrivateInternalExecuteTaskFunction::
    FileManagerPrivateInternalExecuteTaskFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteTaskFunction::Run() {
  using api_fmp_internal::ExecuteTask::Params;
  using api_fmp_internal::ExecuteTask::Results::Create;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::file_tasks::TaskType task_type =
      file_manager::file_tasks::StringToTaskType(params->descriptor.task_type);
  if (task_type == file_manager::file_tasks::TASK_TYPE_UNKNOWN) {
    return RespondNow(Error(kInvalidTaskType + params->descriptor.task_type));
  }
  file_manager::file_tasks::TaskDescriptor task(
      params->descriptor.app_id, task_type, params->descriptor.action_id);

  if (params->urls.empty()) {
    return RespondNow(ArgumentList(Create(api_fmp::TaskResult::kEmpty)));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  std::vector<storage::FileSystemURL> urls;
  for (const auto& url_param : params->urls) {
    const storage::FileSystemURL url =
        file_system_context->CrackURLInFirstPartyContext(GURL{url_param});
    if (!ash::FileSystemBackend::CanHandleURL(url)) {
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
    api_fmp::TaskResult result,
    std::string failure_reason) {
  auto result_list = api_fmp_internal::ExecuteTask::Results::Create(result);
  if (result == api_fmp::TaskResult::kFailed) {
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
  using api_fmp_internal::GetFileTasks::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->urls.empty()) {
    return RespondNow(Error("No URLs provided"));
  }

  if (params->dlp_source_urls.size() != params->urls.size()) {
    return RespondNow(Error("Mismatching URLs and DLP source URLs provided"));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (const auto& url_param : params->urls) {
    const GURL url{url_param};
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(url));
    if (!ash::FileSystemBackend::CanHandleURL(file_system_url)) {
      continue;
    }
    urls_.push_back(url);
    local_paths_.push_back(file_system_url.path());
  }

  dlp_source_urls_ = std::move(params->dlp_source_urls);

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
      dlp_source_urls_,
      base::BindOnce(
          &FileManagerPrivateInternalGetFileTasksFunction::OnFileTasksListed,
          this));
}

void FileManagerPrivateInternalGetFileTasksFunction::OnFileTasksListed(
    std::unique_ptr<file_manager::file_tasks::ResultingTasks> resulting_tasks) {
  // Convert the tasks into JSON compatible objects.
  std::vector<api_fmp::FileTask> results;
  for (const auto& task : resulting_tasks->tasks) {
    api_fmp::FileTask converted;
    converted.descriptor.app_id = task.task_descriptor.app_id;
    converted.descriptor.task_type =
        TaskTypeToString(task.task_descriptor.task_type);
    converted.descriptor.action_id = task.task_descriptor.action_id;
    if (!task.icon_url.is_empty()) {
      converted.icon_url = task.icon_url.spec();
    }
    converted.title = task.task_title;
    converted.is_default = task.is_default;
    converted.is_generic_file_handler = task.is_generic_file_handler;
    converted.is_dlp_blocked = task.is_dlp_blocked;
    results.push_back(std::move(converted));
  }

  api_fmp::ResultingTasks api_resulting_tasks;

  api_resulting_tasks.tasks = std::move(results);
  if (auto status = RemapPolicyDefaultHandlerStatus(
          resulting_tasks->policy_default_handler_status)) {
    api_resulting_tasks.policy_default_handler_status = *status;
  }

  Respond(ArgumentList(api_fmp_internal::GetFileTasks::Results::Create(
      std::move(api_resulting_tasks))));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSetDefaultTaskFunction::Run() {
  using api_fmp_internal::SetDefaultTask::Params;
  const std::optional<Params> params = Params::Create(args());
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
    return RespondNow(WithArguments(true));
  }

  file_manager::file_tasks::TaskType task_type =
      file_manager::file_tasks::StringToTaskType(params->descriptor.task_type);
  if (task_type == file_manager::file_tasks::TASK_TYPE_UNKNOWN) {
    return RespondNow(Error(kInvalidTaskType + params->descriptor.task_type));
  }
  file_manager::file_tasks::TaskDescriptor descriptor(
      params->descriptor.app_id, task_type, params->descriptor.action_id);

  file_manager::file_tasks::UpdateDefaultTask(profile, descriptor, suffixes,
                                              mime_types);
  return RespondNow(NoArguments());
}

}  // namespace extensions
