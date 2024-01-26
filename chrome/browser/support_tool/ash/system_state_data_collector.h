// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_STATE_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_STATE_DATA_COLLECTOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

class SystemStateDataCollector : public DataCollector {
 public:
  SystemStateDataCollector();
  ~SystemStateDataCollector() override;

  // The extra logs that are not returned by GetFeedbackLogs() call but we want
  // to include. We will request these logs using GetLogs() call.
  static const std::vector<std::string> GetExtraLogNames();

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
  struct SystemLog {
    SystemLog(std::string log, std::set<redaction::PIIType> detected_pii_types);
    ~SystemLog();
    SystemLog(const SystemLog& other);

    // Contents of the system log.
    std::string log;
    // Set of PII types detected in the logs.
    std::set<redaction::PIIType> detected_pii_types;
  };

  static std::pair<PIIMap, std::map<std::string, SystemLog>> DetectPII(
      std::map<std::string, SystemLog> system_logs,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container);

  static std::map<std::string, std::string> RedactPII(
      std::map<std::string, SystemLog> system_logs,
      std::set<redaction::PIIType> pii_types_to_keep,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container);

  void OnGetLog(base::RepeatingClosure barrier_closure,
                std::string log_name,
                std::optional<std::string> log);

  void OnGotAllExtraLogs(
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_containe);

  void OnGetFeedbackLogs(
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      bool success,
      const std::map<std::string, std::string>& logs);

  void OnPIIDetected(
      std::pair<PIIMap, std::map<std::string, SystemLog>> detection_result);

  void OnPIIRedacted(base::FilePath target_directory,
                     DataCollectorDoneCallback on_exported_callback,
                     std::map<std::string, std::string> redacted_logs);

  // Runs `on_exported_callback` when file is written.
  void OnFilesWritten(DataCollectorDoneCallback on_exported_callback,
                      bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  std::map<std::string, SystemLog> system_logs_;
  std::vector<std::string> get_log_errors_;
  DataCollectorDoneCallback data_collector_done_callback_;
  base::WeakPtrFactory<SystemStateDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_SYSTEM_STATE_DATA_COLLECTOR_H_
