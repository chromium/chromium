// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_LOGS_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_LOGS_DATA_COLLECTOR_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

class SystemLogsDataCollector : public DataCollector {
 public:
  // `requested_logs` must be the set of base::FilePaths of the base names of
  // the logs. SystemLogsDataCollector will return all available logs under
  // /var/log file if `requested_logs` is set as empty.
  explicit SystemLogsDataCollector(std::set<base::FilePath> requested_logs);
  ~SystemLogsDataCollector() override;

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

 private:
  void OnGetFeedbackLogs(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      bool success,
      const std::map<std::string, std::string>& logs);

  void OnPIIDetected(DataCollectorDoneCallback on_data_collected_callback,
                     std::optional<SupportToolError> error,
                     PIIMap detected_pii);

  void OnPIIRedacted(base::FilePath target_directory,
                     DataCollectorDoneCallback on_exported_callback,
                     std::map<std::string, std::string> redacted_logs);

  void OnFilesWritten(DataCollectorDoneCallback on_exported_callback,
                      bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  std::set<base::FilePath> requested_logs_;
  std::map<std::string, std::string> system_logs_;
  PIIMap pii_map_;
  base::WeakPtrFactory<SystemLogsDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_LOGS_DATA_COLLECTOR_H_
