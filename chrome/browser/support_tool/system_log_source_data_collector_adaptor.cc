// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "data_collector_utils.h"

namespace {

// Detects PII sensitive data that `system_logs_response` contains and returns
// the pair of `system_logs_response` and the detected PII map.
// Takes the ownership of `system_logs_response` and returns it back in the
// returned pair.
std::pair<std::unique_ptr<system_logs::SystemLogsResponse>, PIIMap> DetectPII(
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK(system_logs_response);
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in all entries in `system_logs_response` and add the detected
  // PII to `detected_pii`.
  for (const auto& entry : *system_logs_response) {
    PIIMap pii_in_logs = redaction_tool->Detect(entry.second);
    MergePIIMaps(detected_pii, pii_in_logs);
  }
  return std::make_pair(std::move(system_logs_response),
                        std::move(detected_pii));
}

// Redacts PII sensitive data in `system_logs_response` except
// `pii_types_to_keep` and replaces the values in `system_logs_response` with
// redacted versions. Takes the ownership of `system_logs_response` and returns
// it back.
std::unique_ptr<system_logs::SystemLogsResponse> RedactAndKeepSelectedPII(
    const std::set<redaction::PIIType>& pii_types_to_keep,
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK(system_logs_response);
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  for (auto& entry : *system_logs_response) {
    (*system_logs_response)[entry.first] =
        redaction_tool->RedactAndKeepSelected(entry.second, pii_types_to_keep);
  }
  return system_logs_response;
}

// Opens files in `target_directory` and writes entries in
// `system_logs_response` to their respective files. Each file will be named
// with keys of `system_logs_response` and contain the values as contents.
// Takes the ownership of `system_logs_response`. Returns true on success.
bool WriteOutputFiles(
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response,
    base::FilePath target_directory) {
  DCHECK(system_logs_response);
  bool success = true;
  for (auto& entry : *system_logs_response) {
    if (!base::WriteFile(target_directory.AppendASCII(entry.first)
                             .AddExtension(FILE_PATH_LITERAL(".log")),
                         entry.second))
      success = false;
  }
  return success;
}

}  // namespace

SystemLogSourceDataCollectorAdaptor::SystemLogSourceDataCollectorAdaptor(
    std::string description,
    std::unique_ptr<system_logs::SystemLogsSource> log_source)
    : description_(description), log_source_(std::move(log_source)) {}

SystemLogSourceDataCollectorAdaptor::~SystemLogSourceDataCollectorAdaptor() =
    default;

std::string SystemLogSourceDataCollectorAdaptor::GetName() const {
  return log_source_->source_name();
}

std::string SystemLogSourceDataCollectorAdaptor::GetDescription() const {
  return description_;
}

const PIIMap& SystemLogSourceDataCollectorAdaptor::GetDetectedPII() {
  return pii_map_;
}

void SystemLogSourceDataCollectorAdaptor::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  log_source_->Fetch(base::BindOnce(
      &SystemLogSourceDataCollectorAdaptor::OnDataFetched,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_data_collected_callback),
      task_runner_for_redaction_tool, redaction_tool_container));
}

void SystemLogSourceDataCollectorAdaptor::OnDataFetched(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_logs_response_ = std::move(system_logs_response);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, std::move(system_logs_response_),
                     redaction_tool_container),
      base::BindOnce(&SystemLogSourceDataCollectorAdaptor::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void SystemLogSourceDataCollectorAdaptor::OnPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    std::pair<std::unique_ptr<system_logs::SystemLogsResponse>, PIIMap>
        detection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_logs_response_ = std::move(detection_result.first);
  pii_map_ = std::move(detection_result.second);
  std::move(on_data_collected_callback).Run(/*error=*/std::nullopt);
}

void SystemLogSourceDataCollectorAdaptor::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactAndKeepSelectedPII, pii_types_to_keep,
                     std::move(system_logs_response_),
                     redaction_tool_container),
      base::BindOnce(&SystemLogSourceDataCollectorAdaptor::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void SystemLogSourceDataCollectorAdaptor::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Since we won't need `system_logs_response` after writing the output
  // file, it's okay for WriteOutputFiles to take the ownership of
  // `system_logs_response` directly and not return it back.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFiles, std::move(system_logs_response),
                     target_directory),
      base::BindOnce(&SystemLogSourceDataCollectorAdaptor::OnFilesWritten,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void SystemLogSourceDataCollectorAdaptor::OnFilesWritten(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  if (!success) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        base::StrCat({GetName(), "failed on data export."})};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}

void SystemLogSourceDataCollectorAdaptor::SetLogSourceForTesting(
    std::unique_ptr<system_logs::SystemLogsSource> log_source) {
  log_source_ = std::move(log_source);
}
