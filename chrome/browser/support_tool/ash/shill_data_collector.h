// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_SHILL_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_SHILL_DATA_COLLECTOR_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/atomic_ref_count.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"

// ShillDataCollector collects support debug data from shill. It detects
// shill-specific PII and it also uses redaction::RedactionTool to further remove
// other common types of PII. This class originates from
// system_logs::ShillLogSource.
class ShillDataCollector : public DataCollector {
 public:
  ShillDataCollector();
  ~ShillDataCollector() override;

  ShillDataCollector(const ShillDataCollector&) = delete;
  ShillDataCollector& operator=(const ShillDataCollector&) = delete;
  ShillDataCollector(ShillDataCollector&&) = delete;
  ShillDataCollector& operator=(ShillDataCollector&&) = delete;

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
  // Will be called after the shill log source has been retrieved and PII
  // detection has been performed. Inserts detected PII into `pii_map_`
  // of this.
  void OnPIIDetected(PIIMap detected_pii);

  // Will be called after the shill log has been converted into the json format
  // and PII in it has been masked. Posts a task to run `WriteOutputFiles`.
  void OnPIIRedacted(base::FilePath target_directory,
                     DataCollectorDoneCallback on_exported_callback,
                     std::string shill_property);

  // Will be called after output files have been written. Returns an error to
  // the callback if `success` is false. Clears `shill_log_`.
  void OnFilesWritten(base::FilePath target_directory,
                      DataCollectorDoneCallback on_exported_callback,
                      bool success);

  // These are functions originated from `system_logs::ShillLogSource`.
  void OnGetManagerProperties(std::optional<base::Value::Dict> result);

  void OnGetDevice(const std::string& device_path,
                   std::optional<base::Value::Dict> properties);

  void AddDeviceAndRequestIPConfigs(const std::string& device_path,
                                    const base::Value::Dict& properties);

  void OnGetIPConfig(const std::string& device_path,
                     const std::string& ip_config_path,
                     std::optional<base::Value::Dict> properties);

  void AddIPConfig(const std::string& device_path,
                   const std::string& ip_config_path,
                   const base::Value::Dict& properties);

  void OnGetService(const std::string& service_path,
                    std::optional<base::Value::Dict> properties);

  // Expands UIData from JSON into a dictionary if present. Also detects PII
  // such as the device and service names.
  base::Value::Dict ExpandProperties(const std::string& object_path,
                                     const base::Value::Dict& properties);

  // Check whether all property requests have been completed. If so, runs
  // redaction::RedactionTool on the collected log.
  void CheckIfDone();

  SEQUENCE_CHECKER(sequence_checker_);
  // Store the parameters passed in with `CollectDataAndDetectPII`.
  DataCollectorDoneCallback data_collector_done_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
  // Contains the retrieved shill log.
  base::Value::Dict shill_log_;
  PIIMap pii_map_;
  // Records the number of pending entries to be processed.
  int num_entries_left_;
  // Contains errors encountered when retrieving Shill logs.
  std::map<std::string, std::vector<std::string>> collector_err_;
  base::WeakPtrFactory<ShillDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_SHILL_DATA_COLLECTOR_H_
