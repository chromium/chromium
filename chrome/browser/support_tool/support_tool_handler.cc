// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_handler.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/support_packet_metadata.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "data_collector_utils.h"
#include "third_party/zlib/google/zip.h"

// Zip archieves the contents of `src_path` into `target_path`. Adds ".zip"
// extension to target file path. Returns the path of zip archive on success, an
// empty path otherwise.
base::FilePath ZipOutput(base::FilePath src_path, base::FilePath target_path) {
  base::FilePath zip_path = target_path.AddExtension(FILE_PATH_LITERAL(".zip"));
  if (!zip::Zip(src_path, zip_path, true)) {
    LOG(ERROR) << "Couldn't zip files";
    return base::FilePath();
  }
  return zip_path;
}

// Creates a unique temp directory to store the output files. The caller is
// responsible for deleting the returned directory. Returns an empty FilePath in
// case of an error.
base::FilePath CreateTempDirForOutput() {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(ERROR) << "Unable to create temp dir.";
    return base::FilePath{};
  }
  return temp_dir.Take();
}

SupportToolHandler::SupportToolHandler()
    : SupportToolHandler(/*case_id=*/std::string(),
                         /*email_address=*/std::string(),
                         /*issue_description=*/std::string(),
                         std::nullopt) {}

SupportToolHandler::SupportToolHandler(std::string case_id,
                                       std::string email_address,
                                       std::string issue_description,
                                       std::optional<std::string> upload_id)
    : metadata_(case_id, email_address, issue_description, upload_id),
      task_runner_for_redaction_tool_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      redaction_tool_container_(
          base::MakeRefCounted<redaction::RedactionToolContainer>(
              task_runner_for_redaction_tool_,
              nullptr)) {}

SupportToolHandler::~SupportToolHandler() {
  CleanUp();
}

void SupportToolHandler::CleanUp() {
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

const std::string& SupportToolHandler::GetCaseId() {
  return metadata_.GetCaseId();
}

const base::Time& SupportToolHandler::GetDataCollectionTimestamp() {
  return data_collection_timestamp_;
}

void SupportToolHandler::AddDataCollector(
    std::unique_ptr<DataCollector> collector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(collector);
  data_collectors_.emplace_back(std::move(collector));
}

const std::vector<std::unique_ptr<DataCollector>>&
SupportToolHandler::GetDataCollectorsForTesting() {
  CHECK_IS_TEST();
  return data_collectors_;
}

void SupportToolHandler::CollectSupportData(
    SupportToolDataCollectedCallback on_data_collection_done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_data_collection_done_callback.is_null());
  DCHECK(!data_collectors_.empty());

  on_data_collection_done_callback_ =
      std::move(on_data_collection_done_callback);

  base::RepeatingClosure collect_data_barrier_closure = base::BarrierClosure(
      data_collectors_.size(),
      base::BindOnce(&SupportToolHandler::OnAllDataCollected,
                     weak_ptr_factory_.GetWeakPtr()));

  data_collection_timestamp_ = base::Time::NowFromSystemTime();

  for (auto& data_collector : data_collectors_) {
    // DataCollectors will use `redaction_tool_container_` on
    // `task_runner_for_redaction_tool_` to redact PII from the collected logs.
    // All DataCollectors will use the same RedactionTool instance on the same
    // task runner as we need to replace the same PII data with the same
    // place-holder strings (that are stored in RedactionTool instance's data
    // member) in all collected logs to avoid confusing the reader.
    data_collector->CollectDataAndDetectPII(
        base::BindOnce(&SupportToolHandler::OnDataCollected,
                       weak_ptr_factory_.GetWeakPtr(),
                       collect_data_barrier_closure),
        task_runner_for_redaction_tool_, redaction_tool_container_);
  }
}

void SupportToolHandler::OnDataCollected(
    base::RepeatingClosure barrier_closure,
    std::optional<SupportToolError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    collected_errors_.insert(error.value());
  }
  std::move(barrier_closure).Run();
}

void SupportToolHandler::AddDetectedPII(const PIIMap& pii_map) {
  MergePIIMaps(detected_pii_, pii_map);
}

void SupportToolHandler::OnAllDataCollected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& data_collector : data_collectors_) {
    AddDetectedPII(data_collector->GetDetectedPII());
  }

  metadata_.InsertErrors(collected_errors_);

  metadata_.PopulateMetadataContents(
      data_collection_timestamp_, data_collectors_,
      base::BindOnce(&SupportToolHandler::OnMetadataContentsPopulated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolHandler::OnMetadataContentsPopulated() {
  AddDetectedPII(metadata_.GetPII());
  std::move(on_data_collection_done_callback_)
      .Run(detected_pii_, collected_errors_);
}

void SupportToolHandler::ExportCollectedData(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_path,
    SupportToolDataExportedCallback on_data_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear the set of previously collected errors.
  collected_errors_.clear();

  on_data_export_done_callback_ = std::move(on_data_exported_callback);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&CreateTempDirForOutput),
      base::BindOnce(&SupportToolHandler::ExportIntoTempDir,
                     weak_ptr_factory_.GetWeakPtr(), pii_types_to_keep,
                     target_path));
}

void SupportToolHandler::ExportIntoTempDir(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_path,
    base::FilePath tmp_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (tmp_path.empty()) {
    collected_errors_.insert(
        {SupportToolErrorCode::kDataExportError,
         "Failed to create temporary directory for output."});
    std::move(on_data_export_done_callback_)
        .Run(base::FilePath(), collected_errors_);
    return;
  }

  temp_dir_ = tmp_path;

  base::RepeatingClosure export_data_barrier_closure = base::BarrierClosure(
      data_collectors_.size(),
      base::BindOnce(&SupportToolHandler::OnAllDataCollectorsDoneExporting,
                     weak_ptr_factory_.GetWeakPtr(), temp_dir_, target_path,
                     pii_types_to_keep));

  for (auto& data_collector : data_collectors_) {
    data_collector->ExportCollectedDataWithPII(
        pii_types_to_keep, temp_dir_, task_runner_for_redaction_tool_,
        redaction_tool_container_,
        base::BindOnce(&SupportToolHandler::OnDataCollectorDoneExporting,
                       weak_ptr_factory_.GetWeakPtr(),
                       export_data_barrier_closure));
  }
}

void SupportToolHandler::OnDataCollectorDoneExporting(
    base::RepeatingClosure barrier_closure,
    std::optional<SupportToolError> error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    collected_errors_.insert(error.value());
  }
  std::move(barrier_closure).Run();
}

void SupportToolHandler::OnAllDataCollectorsDoneExporting(
    base::FilePath tmp_path,
    base::FilePath target_path,
    std::set<redaction::PIIType> pii_types_to_keep) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_.InsertErrors(collected_errors_);
  metadata_.WriteMetadataFile(
      tmp_path, pii_types_to_keep,
      base::BindOnce(&SupportToolHandler::OnMetadataFileWritten,
                     weak_ptr_factory_.GetWeakPtr(), tmp_path, target_path));
}

void SupportToolHandler::OnMetadataFileWritten(base::FilePath tmp_path,
                                               base::FilePath target_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Archive the contents in the `tmp_path` into `target_path` in a zip file.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ZipOutput, tmp_path, target_path),
      base::BindOnce(&SupportToolHandler::OnDataExportDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SupportToolHandler::OnDataExportDone(base::FilePath exported_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Clean-up the temporary directory after exporting the data.
  CleanUp();
  if (exported_path.empty()) {
    collected_errors_.insert({SupportToolErrorCode::kDataExportError,
                              "Failed to archive the output files."});
  }
  std::move(on_data_export_done_callback_)
      .Run(exported_path, collected_errors_);
}
