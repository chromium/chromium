// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_CHROME_USER_LOGS_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_CHROME_USER_LOGS_DATA_COLLECTOR_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

// Collects logs in primary user's profile directory. Please note that
// `ChromeUserLogsDataCollector` won't work on sign-in screen.
class ChromeUserLogsDataCollector : public DataCollector {
 public:
  ChromeUserLogsDataCollector();
  ~ChromeUserLogsDataCollector() override;

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
  void CleanUp();

  void OnTempDirCreated(std::optional<base::FilePath> temp_dir);

  void OnGetUserLogPaths(std::vector<base::FilePath> user_logs_paths);

  void OnUserLogFileRead(base::RepeatingClosure barrier_closure,
                         base::FilePath original_log_path,
                         std::pair<base::FilePath, std::string> user_log_file);

  void OnPIIDetected(base::RepeatingClosure barrier_closure,
                     base::FilePath path_in_temp_dir,
                     PIIMap detected_pii);

  void OnAllUserLogFilesReadAndDetected();

  void OnReadLogFromFile(
      base::RepeatingClosure barrier_closure,
      std::string file_name,
      base::FilePath target_directory,
      std::set<redaction::PIIType> pii_types_to_keep,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      std::optional<std::string> log_contents);

  void OnPIIRedacted(base::RepeatingClosure barrier_closure,
                     std::string file_name,
                     base::FilePath target_directory,
                     std::string redacted_log);

  void OnLogFileWritten(base::RepeatingClosure barrier_closure,
                        std::string file_name,
                        bool success);

  void OnAllLogFilesWritten();

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  base::FilePath temp_dir_;
  std::map<base::FilePath, std::set<redaction::PIIType>> pii_in_log_files_;
  std::vector<std::string> errors_;
  DataCollectorDoneCallback on_data_collector_done_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
  base::WeakPtrFactory<ChromeUserLogsDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_CHROME_USER_LOGS_DATA_COLLECTOR_H_
