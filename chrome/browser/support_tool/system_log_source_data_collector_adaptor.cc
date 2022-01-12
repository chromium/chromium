// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/pii_types.h"
#include "components/feedback/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Detects PII sensitive data that `system_logs_response` contains and returns
// the map of detected data.
PIIMap DetectPII(
    system_logs::SystemLogsResponse* system_logs_response,
    scoped_refptr<feedback::RedactionToolContainer> redaction_tool_container) {
  DCHECK(system_logs_response);
  feedback::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in all entries in `system_logs_response` and add the detected
  // PII to `detected_pii`.
  for (const auto& entry : *system_logs_response) {
    PIIMap pii_in_logs = redaction_tool->Detect(entry.second);
    for (auto& pii_data : pii_in_logs) {
      detected_pii[pii_data.first].insert(pii_data.second.begin(),
                                          pii_data.second.end());
    }
  }
  return detected_pii;
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
    scoped_refptr<feedback::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  log_source_->Fetch(base::BindOnce(
      &SystemLogSourceDataCollectorAdaptor::OnDataFetched,
      weak_ptr_factory_.GetWeakPtr(), std::move(on_data_collected_callback),
      task_runner_for_redaction_tool, redaction_tool_container));
}

void SystemLogSourceDataCollectorAdaptor::OnDataFetched(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<feedback::RedactionToolContainer> redaction_tool_container,
    std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  system_logs_response_ = std::move(system_logs_response);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, system_logs_response_.get(),
                     redaction_tool_container),
      base::BindOnce(&SystemLogSourceDataCollectorAdaptor::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void SystemLogSourceDataCollectorAdaptor::OnPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    PIIMap detected_pii) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pii_map_ = std::move(detected_pii);
  std::move(on_data_collected_callback).Run(/*error_code=*/absl::nullopt);
}

void SystemLogSourceDataCollectorAdaptor::ExportCollectedDataWithPII(
    std::set<feedback::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Will add implementation of this function in the follow-up CL.
}
