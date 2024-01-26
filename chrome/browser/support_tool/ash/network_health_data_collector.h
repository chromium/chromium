// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_HEALTH_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_HEALTH_DATA_COLLECTOR_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

// Collects network health snapshot and diagnostic through
// `system_log::NetworkHealthSource`.
class NetworkHealthDataCollector : public SystemLogSourceDataCollectorAdaptor {
 public:
  NetworkHealthDataCollector();
  ~NetworkHealthDataCollector() override;

  // Overrides from SystemLogSourceDataCollectorAdaptor.
  // Calls SystemLogSourceDataCollectorAdaptor's function and does extra PII
  // detection on the returned data for network names as RedactionTool can't
  // detect network names.
  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container)
      override;

  // Anonymises the network names in the collected data if `pii_types_to_keep`
  // doesn't contain redaction::PIIType::kStableIdentifiers as network names are
  // categorised under it and RedactionTool can't remove them. Calls
  // `SystemLogSourceDataCollectorAdaptor::ExportCollectedDataWithPII` function
  // after removing network names.
  void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) override;

 private:
  void OnSystemLogSourceDataCollectorAdaptorCollectedData(
      DataCollectorDoneCallback on_data_collected_callback,
      std::optional<SupportToolError> error);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NetworkHealthDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_HEALTH_DATA_COLLECTOR_H_
