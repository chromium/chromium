// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/open_util.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
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

namespace file_manager::util {
namespace {

bool shell_operations_allowed = true;

// Executes the |task| for the file specified by |url|.
void ExecuteFileTaskForUrl(Profile* profile,
                           const file_tasks::TaskDescriptor& task,
                           const GURL& url) {
  if (!shell_operations_allowed) {
    return;
  }
  storage::FileSystemContext* file_system_context =
      GetFileManagerFileSystemContext(profile);

  file_tasks::ExecuteFileTask(
      profile, task,
      std::vector<FileSystemURL>(
          1, file_system_context->CrackURLInFirstPartyContext(url)),
      base::DoNothing());
}

// Opens the file manager for |url|. Files app will open to the given folder if
// |url| is a folder, or open the parent folder and select the file if |url| is
// a file.
void OpenFileManager(Profile* profile, const GURL& url) {
  if (!shell_operations_allowed) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("ShowFileBrowserFullTab"));

  storage::FileSystemContext* file_system_context =
      GetFileManagerFileSystemContext(profile);

  const GURL destination_entry =
      file_system_context->CrackURLInFirstPartyContext(url).ToGURL();
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  GURL files_swa_url =
      ::file_manager::util::GetFileManagerMainPageUrlWithParams(
          ui::SelectFileDialog::SELECT_NONE, /*title=*/{},
          /*current_directory_url=*/{},
          /*selection_url=*/destination_entry,
          /*target_name=*/{}, &file_type_info,
          /*file_type_index=*/0,
          /*search_query=*/{},
          /*show_android_picker_apps=*/false,
          /*volume_filter=*/{});

  ash::SystemAppLaunchParams params;
  params.url = files_swa_url;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               params);
}

void OpenFileMimeTypeAfterTasksListed(
    Profile* profile,
    const GURL& url,
    platform_util::OpenOperationCallback callback,
    std::unique_ptr<file_tasks::ResultingTasks> resulting_tasks) {
  // Select a default handler. If a default handler is not available, select
  // the first non-generic file handler.
  const file_tasks::FullTaskDescriptor* chosen_task = nullptr;
  for (const auto& task : resulting_tasks->tasks) {
    if (!task.is_generic_file_handler) {
      if (task.is_default) {
        chosen_task = &task;
        break;
      }
      if (!chosen_task) {
        chosen_task = &task;
      }
    }
  }

  if (chosen_task != nullptr) {
    if (shell_operations_allowed) {
      ExecuteFileTaskForUrl(profile, chosen_task->task_descriptor, url);
    }
    std::move(callback).Run(platform_util::OPEN_SUCCEEDED);
  } else {
    std::move(callback).Run(
        platform_util::OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE);
  }
}

// Opens the file with fetched MIME type and calls the callback.
void OpenFileWithMimeType(Profile* profile,
                          const base::FilePath& path,
                          const GURL& url,
                          platform_util::OpenOperationCallback callback,
                          const std::string& mime_type) {
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(path, mime_type, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(url);

  file_tasks::FindAllTypesOfTasks(
      profile, entries, file_urls, {""},
      base::BindOnce(&OpenFileMimeTypeAfterTasksListed, profile, url,
                     std::move(callback)));
}

// Opens the file specified by |url| by finding and executing a file task for
// the file. Calls |callback| with the result.
void OpenFile(Profile* profile,
              const base::FilePath& path,
              const GURL& url,
              platform_util::OpenOperationCallback callback) {
  extensions::app_file_handler_util::GetMimeTypeForLocalPath(
      profile, path,
      base::BindOnce(&OpenFileWithMimeType, profile, path, url,
                     std::move(callback)));
}

void OpenItemWithMetadata(Profile* profile,
                          const base::FilePath& file_path,
                          const GURL& url,
                          platform_util::OpenItemType expected_type,
                          platform_util::OpenOperationCallback callback,
                          base::File::Error error,
                          const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error == base::File::FILE_ERROR_NOT_FOUND
                                ? platform_util::OPEN_FAILED_PATH_NOT_FOUND
                                : platform_util::OPEN_FAILED_FILE_ERROR);
    return;
  }

  // Note that there exists a TOCTOU race between the time the metadata for
  // |file_path| was determined and when it is opened based on the metadata.
  if (expected_type == platform_util::OPEN_FOLDER && file_info.is_directory) {
    OpenFileManager(profile, url);
    std::move(callback).Run(platform_util::OPEN_SUCCEEDED);
    return;
  }

  if (expected_type == platform_util::OPEN_FILE && !file_info.is_directory) {
    OpenFile(profile, file_path, url, std::move(callback));
    return;
  }

  std::move(callback).Run(platform_util::OPEN_FAILED_INVALID_TYPE);
}

void ShowItemInFolderWithMetadata(Profile* profile,
                                  const base::FilePath& file_path,
                                  const GURL& url,
                                  platform_util::OpenOperationCallback callback,
                                  base::File::Error error,
                                  const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error == base::File::FILE_ERROR_NOT_FOUND
                                ? platform_util::OPEN_FAILED_PATH_NOT_FOUND
                                : platform_util::OPEN_FAILED_FILE_ERROR);
    return;
  }

  // This action changes the selection so we do not reuse existing tabs.
  OpenFileManager(profile, url);
  std::move(callback).Run(platform_util::OPEN_SUCCEEDED);
}

}  // namespace

void OpenItem(Profile* profile,
              const base::FilePath& file_path,
              platform_util::OpenItemType expected_type,
              platform_util::OpenOperationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This is unfortunately necessary as file browser handlers operate on URLs.
  GURL url;
  if (!ConvertAbsoluteFilePathToFileSystemUrl(profile, file_path,
                                              GetFileManagerURL(), &url)) {
    std::move(callback).Run(platform_util::OPEN_FAILED_PATH_NOT_FOUND);
    return;
  }

  GetMetadataForPath(
      GetFileManagerFileSystemContext(profile), file_path,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&OpenItemWithMetadata, profile, file_path, url,
                     expected_type, std::move(callback)));
}

void ShowItemInFolder(Profile* profile,
                      const base::FilePath& file_path,
                      platform_util::OpenOperationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Convert Fusebox paths if possible.
  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  storage::FileSystemURL file_url;
  if (fusebox_server) {
    file_url = fusebox_server->ResolveFilename(profile, file_path.value());
  }

  GURL url;
  if (file_url.is_valid()) {
    url = file_url.ToGURL();
  } else if (!ConvertAbsoluteFilePathToFileSystemUrl(
                 profile, file_path, GetFileManagerURL(), &url)) {
    std::move(callback).Run(platform_util::OPEN_FAILED_PATH_NOT_FOUND);
    return;
  }

  GetMetadataForPath(
      GetFileManagerFileSystemContext(profile), file_path,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&ShowItemInFolderWithMetadata, profile, file_path, url,
                     std::move(callback)));
}

void DisableShellOperationsForTesting() {
  shell_operations_allowed = false;
}

}  // namespace file_manager::util
