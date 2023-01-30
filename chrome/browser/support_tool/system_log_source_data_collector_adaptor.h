// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SYSTEM_LOG_SOURCE_DATA_COLLECTOR_ADAPTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SYSTEM_LOG_SOURCE_DATA_COLLECTOR_ADAPTOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"

// SystemLogSourceDataCollectorAdaptor is an adaptor class from
// system_logs::SystemLogsSource to DataCollector. It will take a
// system_logs::SystemLogsSource instance and will use its functions in
// DataCollector interface. SystemLogSourceDataCollectorAdaptor will use
// redaction::RedactionTool for detection and removal of PII sensitive data.
// Since this is an adaptor DataCollector, it will have many instances and the
// caller of the class needs to give `description` as parameter to describe what
// each SystemLogSourceDataCollector instance does in detail.
class SystemLogSourceDataCollectorAdaptor : public DataCollector {
 public:
  SystemLogSourceDataCollectorAdaptor(
      std::string description,
      std::unique_ptr<system_logs::SystemLogsSource> log_source);

  ~SystemLogSourceDataCollectorAdaptor() override;

  // Overrides from DataCollector.
  std::string GetName() const override;

  std::string GetDescription() const override;

  const PIIMap& GetDetectedPII() override;

  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container)
      override;

  void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) override;

  void SetLogSourceForTesting(
      std::unique_ptr<system_logs::SystemLogsSource> log_source);

 protected:
  // The response that the SystemLogsSource returned. Contains a map
  // std::strings that contains the collected logs.
  std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response_;
  PIIMap pii_map_;

 private:
  // Will be called when `log_source_` is done with its Fetch() function.
  void OnDataFetched(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response);

  void OnPIIDetected(DataCollectorDoneCallback on_data_collected_callback,
                     std::pair<std::unique_ptr<system_logs::SystemLogsResponse>,
                               PIIMap> detection_result);

  void OnPIIRedacted(
      base::FilePath target_directory,
      DataCollectorDoneCallback on_exported_callback,
      std::unique_ptr<system_logs::SystemLogsResponse> system_logs_response);

  void OnFilesWritten(base::FilePath target_directory,
                      DataCollectorDoneCallback on_exported_callback,
                      bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  // Description for the DataCollector.
  std::string description_;
  std::unique_ptr<system_logs::SystemLogsSource> log_source_;
  base::WeakPtrFactory<SystemLogSourceDataCollectorAdaptor> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SYSTEM_LOG_SOURCE_DATA_COLLECTOR_ADAPTOR_H_
