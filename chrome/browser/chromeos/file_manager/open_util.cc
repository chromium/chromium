// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/open_util.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace file_manager {
namespace util {
namespace {

bool shell_operations_allowed = true;

void IgnoreFileTaskExecuteResult(
    extensions::api::file_manager_private::TaskResult result,
    std::string failure_reason) {}

// Executes the |task| for the file specified by |url|.
void ExecuteFileTaskForUrl(Profile* profile,
                           const file_tasks::TaskDescriptor& task,
                           const GURL& url) {
  if (!shell_operations_allowed)
    return;
  storage::FileSystemContext* file_system_context =
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId);

  file_tasks::ExecuteFileTask(
      profile,
      GetFileManagerMainPageUrl(),  // Executing task on behalf of the Files
                                    // app.
      task, std::vector<FileSystemURL>(1, file_system_context->CrackURL(url)),
      base::BindOnce(&IgnoreFileTaskExecuteResult));
}

// Opens the file manager for the specified |url|. Used to implement
// internal handlers of special action IDs:
//
// "open" - Open the file manager for the given folder.
// "select" - Open the file manager for the given file. The folder containing
//            the file will be opened with the file selected.
void OpenFileManagerWithInternalActionId(Profile* profile,
                                         const GURL& url,
                                         const std::string& action_id) {
  DCHECK(action_id == "open" || action_id == "select");
  if (!shell_operations_allowed)
    return;
  base::RecordAction(base::UserMetricsAction("ShowFileBrowserFullTab"));

  file_tasks::TaskDescriptor task(
      kFileManagerAppId, file_tasks::TASK_TYPE_FILE_HANDLER, action_id);
  ExecuteFileTaskForUrl(profile, task, url);
}

void OpenFileMimeTypeAfterTasksListed(
    Profile* profile,
    const GURL& url,
    const platform_util::OpenOperationCallback& callback,
    std::unique_ptr<std::vector<file_tasks::FullTaskDescriptor>> tasks) {
  // Select a default handler. If a default handler is not available, select
  // the first non-generic file handler.
  const file_tasks::FullTaskDescriptor* chosen_task = nullptr;
  for (const auto& task : *tasks) {
    if (!task.is_generic_file_handler()) {
      if (task.is_default()) {
        chosen_task = &task;
        break;
      }
      if (!chosen_task)
        chosen_task = &task;
    }
  }

  if (chosen_task != nullptr) {
    if (shell_operations_allowed)
      ExecuteFileTaskForUrl(profile, chosen_task->task_descriptor(), url);
    callback.Run(platform_util::OPEN_SUCCEEDED);
  } else {
    callback.Run(platform_util::OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE);
  }
}

// Opens the file with fetched MIME type and calls the callback.
void OpenFileWithMimeType(Profile* profile,
                          const base::FilePath& path,
                          const GURL& url,
                          const platform_util::OpenOperationCallback& callback,
                          const std::string& mime_type) {
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(path, mime_type, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  file_tasks::FindAllTypesOfTasks(
      profile, entries, file_urls,
      base::BindOnce(&OpenFileMimeTypeAfterTasksListed, profile, url,
                     callback));
}

// Opens the file specified by |url| by finding and executing a file task for
// the file. Calls |callback| with the result.
void OpenFile(Profile* profile,
              const base::FilePath& path,
              const GURL& url,
              const platform_util::OpenOperationCallback& callback) {
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile, path,
      base::Bind(&OpenFileWithMimeType, profile, path, url, callback));
}

void OpenItemWithMetadata(Profile* profile,
                          const base::FilePath& file_path,
                          const GURL& url,
                          platform_util::OpenItemType expected_type,
                          const platform_util::OpenOperationCallback& callback,
                          base::File::Error error,
                          const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    callback.Run(error == base::File::FILE_ERROR_NOT_FOUND
                     ? platform_util::OPEN_FAILED_PATH_NOT_FOUND
                     : platform_util::OPEN_FAILED_FILE_ERROR);
    return;
  }

  // Note that there exists a TOCTOU race between the time the metadata for
  // |file_path| was determined and when it is opened based on the metadata.
  if (expected_type == platform_util::OPEN_FOLDER && file_info.is_directory) {
    OpenFileManagerWithInternalActionId(profile, url, "open");
    callback.Run(platform_util::OPEN_SUCCEEDED);
    return;
  }

  if (expected_type == platform_util::OPEN_FILE && !file_info.is_directory) {
    OpenFile(profile, file_path, url, callback);
    return;
  }

  callback.Run(platform_util::OPEN_FAILED_INVALID_TYPE);
}

void ShowItemInFolderWithMetadata(
    Profile* profile,
    const base::FilePath& file_path,
    const GURL& url,
    const platform_util::OpenOperationCallback& callback,
    base::File::Error error,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    callback.Run(error == base::File::FILE_ERROR_NOT_FOUND
                     ? platform_util::OPEN_FAILED_PATH_NOT_FOUND
                     : platform_util::OPEN_FAILED_FILE_ERROR);
    return;
  }

  // This action changes the selection so we do not reuse existing tabs.
  OpenFileManagerWithInternalActionId(profile, url, "select");
  callback.Run(platform_util::OPEN_SUCCEEDED);
}

}  // namespace

void OpenItem(Profile* profile,
              const base::FilePath& file_path,
              platform_util::OpenItemType expected_type,
              const platform_util::OpenOperationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is unfortunately necessary as file browser handlers operate on URLs.
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(profile, file_path,
                                              kFileManagerAppId, &url)) {
    callback.Run(platform_util::OPEN_FAILED_PATH_NOT_FOUND);
    return;
  }

  GetMetadataForPath(
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId), file_path,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
      base::Bind(&OpenItemWithMetadata, profile, file_path, url, expected_type,
                 callback));
}

void ShowItemInFolder(Profile* profile,
                      const base::FilePath& file_path,
                      const platform_util::OpenOperationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(profile, file_path,
                                              kFileManagerAppId, &url)) {
    callback.Run(platform_util::OPEN_FAILED_PATH_NOT_FOUND);
    return;
  }

  GetMetadataForPath(
      GetFileSystemContextForExtensionId(profile, kFileManagerAppId), file_path,
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
      base::BindOnce(&ShowItemInFolderWithMetadata, profile, file_path, url,
                     callback));
}

void DisableShellOperationsForTesting() {
  shell_operations_allowed = false;
}

}  // namespace util
}  // namespace file_manager
