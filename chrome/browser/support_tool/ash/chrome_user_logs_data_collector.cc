// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/chrome_user_logs_data_collector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

// The directory where chrome logs exist under user's
// profile directory.
constexpr char kChromeLogsDir[] = "log";
// The pattern for name of Chrome logs file.
constexpr char kChromeLogsPattern[] = "chrome*";

// Paths of other (non-Chrome) user logs.
constexpr std::array<const char*, 3> kOtherLogsPaths = {
    "google-assistant-library/log/libassistant.log", "login-times",
    "logout-times"};

// Creates a temporary directory and returns it if there's no error. Gives the
// ownership of the temporary directory to the caller and caller will be
// responsible of deleting it.
std::optional<base::FilePath> CreateTempDir() {
  base::ScopedTempDir temp_dir;
  if (temp_dir.CreateUniqueTempDir())
    return temp_dir.Take();
  return std::nullopt;
}

std::vector<base::FilePath> GetUserLogPaths(base::FilePath profile_dir) {
  std::vector<base::FilePath> paths;

  // Add Chrome logs.
  base::FilePath chrome_log_dir = profile_dir.AppendASCII(kChromeLogsDir);
  base::FileEnumerator file_enumerator(chrome_log_dir, /*recursive=*/false,
                                       base::FileEnumerator::FILES,
                                       kChromeLogsPattern);
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    paths.push_back(path);
  }

  // Add other user logs.
  for (const auto* log : kOtherLogsPaths) {
    base::FilePath log_path = profile_dir.AppendASCII(log);
    // Check if the path exists because some log files may not exist in some
    // devices.
    if (base::PathExists(log_path))
      paths.push_back(log_path);
  }
  return paths;
}

std::pair<base::FilePath, std::string> ReadUserLogAndCopyContents(
    base::FilePath log_path,
    base::FilePath target_directory) {
  base::FilePath copy_target = target_directory.Append(log_path.BaseName());
  if (!base::CopyFile(log_path, copy_target))
    return {base::FilePath(), std::string()};

  std::string file_contents;
  if (!base::ReadFileToString(log_path, &file_contents))
    return {copy_target, std::string()};

  return {copy_target, file_contents};
}

PIIMap DetectPII(
    std::string log_contents,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  return redaction_tool->Detect(log_contents);
}

std::set<redaction::PIIType> GetPIITypesInMap(const PIIMap& pii_map) {
  std::set<redaction::PIIType> pii_types;
  for (const auto& pii_map_entry : pii_map)
    pii_types.insert(pii_map_entry.first);
  return pii_types;
}

// Adds the contents of `map_to_merge` into `target_map`.
void MergePIIMaps(PIIMap& target_map, PIIMap& map_to_merge) {
  for (auto& pii_data : map_to_merge) {
    target_map[pii_data.first].insert(pii_data.second.begin(),
                                      pii_data.second.end());
  }
}

bool CopyTemporaryLogFileToTarget(base::FilePath log_file,
                                  base::FilePath target_path) {
  return base::Move(log_file, target_path.Append(log_file.BaseName()));
}

std::optional<std::string> ReadLogFromFile(base::FilePath log_file) {
  std::string log;
  if (!base::ReadFileToString(log_file, &log))
    return std::nullopt;
  return log;
}

std::string RedactPII(
    std::string log_contents,
    std::set<redaction::PIIType> pii_types_to_keep,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  return redaction_tool->RedactAndKeepSelected(log_contents, pii_types_to_keep);
}

bool WriteRedactedLogToFile(base::FilePath target_directory,
                            std::string file_name,
                            std::string redacted_log) {
  return base::WriteFile(target_directory.AppendASCII(file_name), redacted_log);
}

}  // namespace

ChromeUserLogsDataCollector::ChromeUserLogsDataCollector() = default;
ChromeUserLogsDataCollector::~ChromeUserLogsDataCollector() {
  CleanUp();
}

void ChromeUserLogsDataCollector::CleanUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clean the temporary directory in a worker thread if it hasn't been removed
  // yet.
  if (!temp_dir_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::GetDeletePathRecursivelyCallback(std::move(temp_dir_)));
    temp_dir_.clear();
  }
}

std::string ChromeUserLogsDataCollector::GetName() const {
  return "Chrome User Logs";
}

std::string ChromeUserLogsDataCollector::GetDescription() const {
  return "Collects Chrome user logs for ChromeOS";
}

const PIIMap& ChromeUserLogsDataCollector::GetDetectedPII() {
  return pii_map_;
}

void ChromeUserLogsDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "A user must have logged in for "
                              "ChromeUserLogsDataCollector."};
    std::move(on_data_collected_callback).Run(error);
    return;
  }

  on_data_collector_done_callback_ = std::move(on_data_collected_callback);
  task_runner_for_redaction_tool_ = task_runner_for_redaction_tool;
  redaction_tool_container_ = redaction_tool_container;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateTempDir),
      base::BindOnce(&ChromeUserLogsDataCollector::OnTempDirCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeUserLogsDataCollector::OnTempDirCreated(
    std::optional<base::FilePath> temp_dir) {
  if (!temp_dir) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "Failed to create temporary directory for "
                              "ChromeUserLogsDataCollector."};
    std::move(on_data_collector_done_callback_).Run(error);
    return;
  }
  temp_dir_ = temp_dir.value();

  // Get primary user's profile path.
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);
  std::string user_hash = user->username_hash();
  DCHECK(!user_hash.empty());
  base::FilePath profile_dir =
      ash::ProfileHelper::GetProfilePathByUserIdHash(user_hash);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&GetUserLogPaths, profile_dir),
      base::BindOnce(&ChromeUserLogsDataCollector::OnGetUserLogPaths,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ChromeUserLogsDataCollector::OnGetUserLogPaths(
    std::vector<base::FilePath> user_logs_paths) {
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      user_logs_paths.size(),
      base::BindOnce(
          &ChromeUserLogsDataCollector::OnAllUserLogFilesReadAndDetected,
          weak_ptr_factory_.GetWeakPtr()));
  for (const auto& path : user_logs_paths) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ReadUserLogAndCopyContents, path, temp_dir_),
        base::BindOnce(&ChromeUserLogsDataCollector::OnUserLogFileRead,
                       weak_ptr_factory_.GetWeakPtr(), barrier_closure, path));
  }
}

void ChromeUserLogsDataCollector::OnUserLogFileRead(
    base::RepeatingClosure barrier_closure,
    base::FilePath original_log_path,
    std::pair<base::FilePath, std::string> user_log_file) {
  if (user_log_file.first.empty()) {
    errors_.push_back(base::StringPrintf(
        "Couldn't copy logs from %s log file",
        original_log_path.BaseName().AsUTF8Unsafe().c_str()));
    std::move(barrier_closure).Run();
    return;
  }
  if (user_log_file.second.empty()) {
    errors_.push_back(base::StringPrintf(
        "Couldn't read logs from %s log file for PII detection",
        original_log_path.BaseName().AsUTF8Unsafe().c_str()));
    std::move(barrier_closure).Run();
    return;
  }
  task_runner_for_redaction_tool_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, user_log_file.second,
                     redaction_tool_container_),
      base::BindOnce(&ChromeUserLogsDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(barrier_closure),
                     user_log_file.first));
}

void ChromeUserLogsDataCollector::OnPIIDetected(
    base::RepeatingClosure barrier_closure,
    base::FilePath path_in_temp_dir,
    PIIMap detected_pii) {
  pii_in_log_files_.emplace(path_in_temp_dir, GetPIITypesInMap(detected_pii));
  MergePIIMaps(pii_map_, detected_pii);
  std::move(barrier_closure).Run();
}

void ChromeUserLogsDataCollector::OnAllUserLogFilesReadAndDetected() {
  task_runner_for_redaction_tool_.reset();
  redaction_tool_container_.reset();
  if (errors_.empty()) {
    std::move(on_data_collector_done_callback_).Run(std::nullopt);
    return;
  }
  SupportToolError error = {
      SupportToolErrorCode::kDataCollectorError,
      base::StringPrintf(
          "ChromeUserLogsDataCollector got errors when reading log files: %s",
          base::JoinString(errors_, ", ").c_str())};
  std::move(on_data_collector_done_callback_).Run(error);
}

void ChromeUserLogsDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_data_collector_done_callback_ = std::move(on_exported_callback);
  // Clear the `errors_` to record the new errors if any occurs.
  errors_.clear();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      pii_in_log_files_.size(),
      base::BindOnce(&ChromeUserLogsDataCollector::OnAllLogFilesWritten,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const auto& [path_in_temp_dir, pii_types] : pii_in_log_files_) {
    bool keep_all_pii_in_log =
        std::includes(pii_types_to_keep.begin(), pii_types_to_keep.end(),
                      pii_types.begin(), pii_types.end());
    // Use `file_name` for detailed error messages.
    std::string file_name = path_in_temp_dir.BaseName().AsUTF8Unsafe();
    if (keep_all_pii_in_log) {
      // Immediately copy the logs from temporary file to `target_directory` if
      // all PII in the logs are selected to be kept.
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&CopyTemporaryLogFileToTarget, path_in_temp_dir,
                         target_directory),
          base::BindOnce(&ChromeUserLogsDataCollector::OnLogFileWritten,
                         weak_ptr_factory_.GetWeakPtr(), barrier_closure,
                         file_name));
      continue;
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ReadLogFromFile, path_in_temp_dir),
        base::BindOnce(&ChromeUserLogsDataCollector::OnReadLogFromFile,
                       weak_ptr_factory_.GetWeakPtr(), barrier_closure,
                       file_name, target_directory, pii_types_to_keep,
                       task_runner_for_redaction_tool,
                       redaction_tool_container));
  }
}

void ChromeUserLogsDataCollector::OnReadLogFromFile(
    base::RepeatingClosure barrier_closure,
    std::string file_name,
    base::FilePath target_directory,
    std::set<redaction::PIIType> pii_types_to_keep,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    std::optional<std::string> log_contents) {
  if (!log_contents) {
    errors_.push_back(base::StringPrintf("Couldn't read logs from %s log file",
                                         file_name.c_str()));
    std::move(barrier_closure).Run();
    return;
  }
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactPII, log_contents.value(), pii_types_to_keep,
                     redaction_tool_container),
      base::BindOnce(&ChromeUserLogsDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(barrier_closure),
                     file_name, target_directory));
}

void ChromeUserLogsDataCollector::OnPIIRedacted(
    base::RepeatingClosure barrier_closure,
    std::string file_name,
    base::FilePath target_directory,
    std::string redacted_log) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteRedactedLogToFile, target_directory, file_name,
                     redacted_log),
      base::BindOnce(&ChromeUserLogsDataCollector::OnLogFileWritten,
                     weak_ptr_factory_.GetWeakPtr(), barrier_closure,
                     file_name));
}

void ChromeUserLogsDataCollector::OnLogFileWritten(
    base::RepeatingClosure barrier_closure,
    std::string file_name,
    bool success) {
  if (!success) {
    errors_.push_back(base::StringPrintf(
        "Couldn't write %s log to target directory", file_name.c_str()));
  }
  std::move(barrier_closure).Run();
}

void ChromeUserLogsDataCollector::OnAllLogFilesWritten() {
  // Clean-up the temporary directory when we're done with file operations.
  CleanUp();
  if (errors_.empty()) {
    std::move(on_data_collector_done_callback_).Run(std::nullopt);
    return;
  }
  SupportToolError error = {
      SupportToolErrorCode::kDataCollectorError,
      base::StringPrintf(
          "ChromeUserLogsDataCollector got errors when exporting log files: %s",
          base::JoinString(errors_, ", ").c_str())};
  std::move(on_data_collector_done_callback_).Run(error);
}
