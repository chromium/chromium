// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/files_cleanup_handler.h"

#include <optional>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {

namespace {

constexpr char kFolderNameDownloads[] = "Downloads";
constexpr char kFolderNameMyFiles[] = "MyFiles";

bool DeleteFilesAndDirectoriesUnderPath(
    const base::FilePath& directory_path,
    const std::optional<base::FilePath>& ignore_path) {
  bool success = true;

  base::FileEnumerator e(
      directory_path, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath file_path = e.Next(); !file_path.empty();
       file_path = e.Next()) {
    if (ignore_path && file_path == *ignore_path)
      continue;

    if (e.GetInfo().IsDirectory()) {
      success &= base::DeletePathRecursively(file_path);
    } else {
      success &= base::DeleteFile(file_path);
    }
  }

  return success;
}

bool EnsureDirectoryIsEmpty(const base::FilePath& directory_path,
                            const base::FilePath& ignore_path) {
  base::FileEnumerator e(
      directory_path, /*recursive=*/true,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
      /*pattern=*/base::FilePath::StringType(),
      base::FileEnumerator::FolderSearchPolicy::MATCH_ONLY,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (base::FilePath file_path = e.Next(); !file_path.empty();
       file_path = e.Next()) {
    if (file_path != ignore_path) {
      return false;
    }
  }

  return e.GetError() == base::File::Error::FILE_OK;
}

}  // namespace

FilesCleanupHandler::FilesCleanupHandler() {
  task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

FilesCleanupHandler::~FilesCleanupHandler() = default;

void FilesCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FilesCleanupHandler::CleanupTaskOnTaskRunner,
                     base::Unretained(this), profile),
      base::BindOnce(&FilesCleanupHandler::CleanupTaskDone,
                     base::Unretained(this), std::move(callback)));
}

bool FilesCleanupHandler::CleanupTaskOnTaskRunner(Profile* profile) {
  base::FilePath my_files_path =
      profile->GetPath().AppendASCII(kFolderNameMyFiles);
  base::FilePath downloads_path =
      my_files_path.AppendASCII(kFolderNameDownloads);

  // Delete all files and directories under MyFiles other than Downloads.
  bool success =
      DeleteFilesAndDirectoriesUnderPath(my_files_path, downloads_path);

  // Delete all files and directories under Downloads.
  success &= DeleteFilesAndDirectoriesUnderPath(downloads_path,
                                                /*ignore_path=*/std::nullopt);

  if (!success) {
    return false;
  }

  // This is needed since `DeleteFilesAndDirectoriesUnderPath()` has
  // `ErrorPolicy::IGNORE_ERRORS`, so we don't know for sure if all files and
  // directories were enumerated.
  return EnsureDirectoryIsEmpty(my_files_path, downloads_path);
}

void FilesCleanupHandler::CleanupTaskDone(CleanupHandlerCallback callback,
                                          bool success) {
  if (!success) {
    std::move(callback).Run("Failed to delete all files and directories");
    return;
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace chromeos
