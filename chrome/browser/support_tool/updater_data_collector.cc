// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/updater_data_collector.h"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/data_collector_utils.h"
#include "chrome/updater/updater_scope.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

namespace {

constexpr auto kUpdaterScopes = {updater::UpdaterScope::kSystem,
                                 updater::UpdaterScope::kUser};

constexpr std::string_view GetScopeName(updater::UpdaterScope scope) {
  return scope == updater::UpdaterScope::kSystem ? "system" : "user";
}

base::expected<PIIMap, SupportToolError> MergeCollectAndDetectResults(
    std::vector<base::expected<PIIMap, SupportToolError>> results) {
  PIIMap pii;
  for (const auto& result : results) {
    if (!result.has_value()) {
      return base::unexpected(result.error());
    }
    MergePIIMaps(pii, *result);
  }
  return pii;
}

std::optional<SupportToolError> GetFirstError(
    const std::vector<std::optional<SupportToolError>>& results) {
  const auto it = std::ranges::find_if(
      results, [](const std::optional<SupportToolError> error) {
        return error.has_value();
      });
  return it == results.end() ? std::nullopt : *it;
}

// Returns whether a file captured by this collector can be redacted. JSON files
// are not redacted as they must remain machine-readable (e.g. to be able to be
// loaded by tools such as chrome://updater).
bool isRedactable(const base::FilePath& filename) {
  return !filename.AsUTF8Unsafe().contains(".json");
}

// Copies a file from the updater's install directory to the provided temporary
// directory and detects PII in its content. Callback is posted on the calling
// sequence with the result of PII detection.
void CollectAndDetectFile(
    base::OnceCallback<void(base::expected<PIIMap, SupportToolError>)> callback,
    const base::FilePath& install_dir,
    const base::FilePath& filename,
    const base::FilePath& temp_path_for_scope,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  const base::FilePath src_path = install_dir.Append(filename);
  if (!base::PathExists(src_path)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), PIIMap()));
    return;
  }
  const base::FilePath dest_path =
      temp_path_for_scope.Append(src_path.BaseName());
  if (!base::CopyFile(src_path, dest_path)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(SupportToolError(
                SupportToolErrorCode::kDataCollectorError,
                base::StrCat({"Failed to copy ", src_path.AsUTF8Unsafe(),
                              " to ", dest_path.AsUTF8Unsafe()})))));
    return;
  }

  std::string contents;
  if (!base::ReadFileToString(dest_path, &contents)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(SupportToolError(
                SupportToolErrorCode::kDataCollectorError,
                base::StrCat({"Failed to read ", dest_path.AsUTF8Unsafe()})))));
    return;
  }

  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<redaction::RedactionToolContainer>
                 redaction_tool_container,
             const std::string& contents, const base::FilePath& filename) {
            return isRedactable(filename)
                       ? redaction_tool_container->Get()->Detect(contents)
                       : PIIMap{};
          },
          redaction_tool_container, std::move(contents), filename),
      std::move(callback));
}

// Copies all of the data from an updater's installation directory which should
// be included in the resulting archive to a temporary directory and detects PII
// in the contents. Callback is posted on the calling sequence once all files
// have been copied and completed PII detection; the resulting PIIMap contains
// the merged entries from all files.
void CollectAndDetectForScope(
    base::OnceCallback<void(base::expected<PIIMap, SupportToolError>)> callback,
    const base::FilePath& install_dir,
    updater::UpdaterScope scope,
    const base::FilePath& temp_root,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  base::FilePath temp_path_for_scope =
      temp_root.AppendASCII(GetScopeName(scope));
  if (!base::CreateDirectory(temp_path_for_scope)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(SupportToolError(
                SupportToolErrorCode::kDataCollectorError,
                base::StrCat({"Failed to create temporary directory ",
                              temp_path_for_scope.AsUTF8Unsafe()})))));
    return;
  }

  static constexpr std::initializer_list<base::FilePath::StringViewType>
      kFileNames = {
          FILE_PATH_LITERAL("updater.log"),
          FILE_PATH_LITERAL("updater.log.old"),
          FILE_PATH_LITERAL("updater_history.jsonl"),
          FILE_PATH_LITERAL("updater_history.jsonl.old"),
          FILE_PATH_LITERAL("prefs.json"),
      };
  base::RepeatingCallback<void(base::expected<PIIMap, SupportToolError>)>
      on_collect_file_complete =
          base::BarrierCallback<base::expected<PIIMap, SupportToolError>>(
              kFileNames.size(), base::BindOnce(&MergeCollectAndDetectResults)
                                     .Then(std::move(callback)));
  for (const base::FilePath::StringViewType& filename : kFileNames) {
    CollectAndDetectFile(on_collect_file_complete, install_dir,
                         base::FilePath(filename), temp_path_for_scope,
                         task_runner_for_redaction_tool,
                         redaction_tool_container);
  }
}

// Reads all of the data staged in a temporary directory, redacts PII, and
// writes the resulting content to the provided output directory for an updater
// scope.
void ExportAndRedactForScope(
    std::set<redaction::PIIType> pii_types_to_keep,
    const base::FilePath& target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback,
    const base::FilePath& temp_root,
    updater::UpdaterScope scope) {
  base::FilePath temp_path_for_scope =
      temp_root.AppendASCII(GetScopeName(scope));
  base::FilePath out_path_for_scope =
      target_directory.AppendASCII(GetScopeName(scope));
  if (!base::CreateDirectory(out_path_for_scope)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_exported_callback),
                       SupportToolError(
                           SupportToolErrorCode::kDataCollectorError,
                           base::StrCat({"Failed to create directory ",
                                         out_path_for_scope.AsUTF8Unsafe()}))));
    return;
  }

  std::vector<base::FilePath> paths;
  base::FileEnumerator(temp_path_for_scope, /*recursive=*/false,
                       base::FileEnumerator::FILES)
      .ForEach([&paths](const base::FilePath& path) { paths.push_back(path); });

  base::RepeatingCallback<void(std::optional<SupportToolError>)>
      barrier_callback = base::BarrierCallback<std::optional<SupportToolError>>(
          paths.size(),
          base::BindOnce(&GetFirstError).Then(std::move(on_exported_callback)));
  for (const base::FilePath& path : paths) {
    std::string contents;
    if (!base::ReadFileToString(path, &contents)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              barrier_callback,
              SupportToolError(SupportToolErrorCode::kDataCollectorError,
                               "Failed to read file")));
      return;
    }
    base::FilePath dest_path = out_path_for_scope.Append(path.BaseName());
    task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](const std::set<redaction::PIIType>& pii_types_to_keep,
               scoped_refptr<redaction::RedactionToolContainer>
                   redaction_tool_container,
               const std::string& contents, const base::FilePath& path) {
              return isRedactable(path.BaseName())
                         ? redaction_tool_container->Get()
                               ->RedactAndKeepSelected(contents,
                                                       pii_types_to_keep)
                         : contents;
            },
            pii_types_to_keep, redaction_tool_container, contents, path),
        base::BindOnce(
            [](const base::FilePath& dest_path,
               const std::string& redacted_contents)
                -> std::optional<SupportToolError> {
              if (!base::WriteFile(dest_path, redacted_contents)) {
                return SupportToolError(
                    SupportToolErrorCode::kDataCollectorError,
                    base::StrCat(
                        {"Failed to write to ", dest_path.AsUTF8Unsafe()}));
              }
              return std::nullopt;
            },
            dest_path)
            .Then(barrier_callback));
  }
}

void CollectDataAndDetectPIIImpl(
    DataCollectorDoneCallback on_data_collected_callback,
    base::OnceCallback<void(const base::FilePath&)> set_temp_dir_callback,
    base::OnceCallback<void(DataCollectorDoneCallback,
                            base::expected<PIIMap, SupportToolError>)>
        on_pii_detected,
    base::FilePath temp_dir,
    const base::FilePath& system_install_dir,
    const base::FilePath& user_install_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  if (temp_dir.empty()) {
    if (!base::CreateNewTempDirectory(
            FILE_PATH_LITERAL("updater_data_collector"), &temp_dir)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(on_data_collected_callback),
              SupportToolError(SupportToolErrorCode::kDataCollectorError,
                               "Failed to create temporary directory")));
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(set_temp_dir_callback), temp_dir));
  }
  base::RepeatingCallback<void(base::expected<PIIMap, SupportToolError>)>
      on_collect_for_scope_complete =
          base::BarrierCallback<base::expected<PIIMap, SupportToolError>>(
              kUpdaterScopes.size(),
              base::BindOnce(&MergeCollectAndDetectResults)
                  .Then(base::BindOnce(std::move(on_pii_detected),
                                       std::move(on_data_collected_callback))));
  for (updater::UpdaterScope scope : kUpdaterScopes) {
    CollectAndDetectForScope(on_collect_for_scope_complete,
                             scope == updater::UpdaterScope::kSystem
                                 ? system_install_dir
                                 : user_install_dir,
                             scope, temp_dir, task_runner_for_redaction_tool,
                             redaction_tool_container);
  }
}

void ExportCollectedDataWithPIIImpl(
    DataCollectorDoneCallback on_exported_callback,
    const base::FilePath& temp_dir,
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  if (!base::PathExists(temp_dir)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_exported_callback),
                                  SupportToolError(
                                      SupportToolErrorCode::kDataCollectorError,
                                      "Missing temporary directory")));
    return;
  }

  target_directory = target_directory.AppendASCII("updater");
  if (!base::CreateDirectory(target_directory)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_exported_callback),
                                  SupportToolError(
                                      SupportToolErrorCode::kDataCollectorError,
                                      "Failed to create directory ")));
    return;
  }

  base::RepeatingCallback<void(std::optional<SupportToolError>)>
      barrier_callback = base::BarrierCallback<std::optional<SupportToolError>>(
          kUpdaterScopes.size(),
          base::BindOnce(&GetFirstError).Then(std::move(on_exported_callback)));
  for (updater::UpdaterScope scope : kUpdaterScopes) {
    ExportAndRedactForScope(
        pii_types_to_keep, target_directory, task_runner_for_redaction_tool,
        redaction_tool_container, barrier_callback, temp_dir, scope);
  }
}

}  // namespace

UpdaterDataCollector::UpdaterDataCollector(
    std::optional<base::FilePath> user_install_dir,
    std::optional<base::FilePath> system_install_dir)
    : user_install_dir_(user_install_dir),
      system_install_dir_(system_install_dir) {}
UpdaterDataCollector::~UpdaterDataCollector() {
  if (!temp_dir_.empty()) {
    io_sequence_->PostTask(FROM_HERE,
                           base::GetDeletePathRecursivelyCallback(temp_dir_));
  }
}

std::string UpdaterDataCollector::GetName() const {
  return "Updater Data Collector";
}

std::string UpdaterDataCollector::GetDescription() const {
  return "Collects diagnostic data for Chrome's auto-updater.";
}

const PIIMap& UpdaterDataCollector::GetDetectedPII() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pii_map_;
}

void UpdaterDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!system_install_dir_ || !user_install_dir_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(on_data_collected_callback),
            SupportToolError(SupportToolErrorCode::kDataCollectorError,
                             "Failed to determine updater install directory")));
    return;
  }

  io_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(&CollectDataAndDetectPIIImpl,
                     base::BindPostTaskToCurrentDefault(
                         std::move(on_data_collected_callback)),
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&UpdaterDataCollector::SetTempDir,
                                        weak_ptr_factory_.GetWeakPtr())),
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&UpdaterDataCollector::OnPIIDetected,
                                        weak_ptr_factory_.GetWeakPtr())),
                     temp_dir_, *system_install_dir_, *user_install_dir_,
                     task_runner_for_redaction_tool, redaction_tool_container));
}

void UpdaterDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  io_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ExportCollectedDataWithPIIImpl,
          base::BindPostTaskToCurrentDefault(std::move(on_exported_callback)),
          temp_dir_, pii_types_to_keep, target_directory,
          task_runner_for_redaction_tool, redaction_tool_container));
}

void UpdaterDataCollector::SetTempDir(const base::FilePath& temp_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  temp_dir_ = temp_dir;
}

void UpdaterDataCollector::OnPIIDetected(
    DataCollectorDoneCallback callback,
    base::expected<PIIMap, SupportToolError> pii_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pii_map.has_value()) {
    pii_map_ = *std::move(pii_map);
    std::move(callback).Run(std::nullopt);
  } else {
    pii_map_.clear();
    std::move(callback).Run(pii_map.error());
  }
}
