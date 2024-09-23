// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/system_logs_writer.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/system_logs/debug_log_writer.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/logging_chrome.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "third_party/zlib/google/zip.h"

namespace ash {

namespace {

// Writes |contents| to a temp directory then compresses it to |dest_file_path|.
// Returns the name of the compressed file (with the zip extension) on success,
// or nullopt on failure.
std::optional<base::FilePath> WriteCompressedFile(
    const std::string& contents,
    base::FilePath dest_file_path) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Unable to create temp dir.";
    return std::nullopt;
  }
  base::FilePath temp_file_path(
      temp_dir.GetPath().Append(dest_file_path.BaseName()));
  if (!base::WriteFile(temp_file_path, contents)) {
    LOG(ERROR) << "Unable to write file: " << temp_file_path.value();
    return std::nullopt;
  }
  base::FilePath zip_file_path(
      dest_file_path.AddExtension(FILE_PATH_LITERAL(".zip")));
  if (!zip::Zip(temp_dir.GetPath(), zip_file_path,
                /*include_hidden_files=*/false)) {
    LOG(ERROR) << "Failed to zip file to: " << zip_file_path.value();
    return std::nullopt;
  }
  return zip_file_path;
}

// Called when SystemLogsFetcher::Fetch completes. Converts |sys_info| into a
// single string using the same mechanism as Feedback reports, then writes the
// string to a compressed system_logs.txt.zip file and invokes |callback|.
void FetchCompleted(
    const base::FilePath& dest_dir,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  base::FilePath system_logs_file_path =
      logging::GenerateTimestampedName(
          dest_dir.Append(FILE_PATH_LITERAL("system_logs")), base::Time::Now())
          .AddExtension(FILE_PATH_LITERAL(".txt"));

  std::string system_logs = feedback_util::LogsToString(*sys_info.get());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(WriteCompressedFile, std::move(system_logs),
                     system_logs_file_path),
      std::move(callback));
}

}  // namespace

namespace system_logs_writer {

void WriteSystemLogs(
    Profile* profile,
    const base::FilePath& dest_dir,
    bool scrub_data,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback) {
  system_logs::BuildChromeSystemLogsFetcher(profile, scrub_data)
      ->Fetch(base::BindOnce(FetchCompleted, dest_dir, std::move(callback)));
}

}  // namespace system_logs_writer
}  // namespace ash
