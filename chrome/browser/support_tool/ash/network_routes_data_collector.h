// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_ROUTES_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_ROUTES_DATA_COLLECTOR_H_

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

// Collects network routing tables data for both IPv4 and IPv6 and writes it
// into "network_routes.txt" file.
class NetworkRoutesDataCollector : public DataCollector {
 public:
  NetworkRoutesDataCollector();
  ~NetworkRoutesDataCollector() override;

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
  // Is called when a GetRoutes() call to DebugDaemonClient succeeds. Checks the
  // contents of `routes` and appends its contents to `network_routes_`.
  void OnGetRoutes(base::RepeatingClosure barrier_closure,
                   std::optional<std::vector<std::string>> routes);

  // Is called when all GetRoutes() calls are done. Runs PII detection on
  // `task_runner_for_redaction_tool` for `network_routes_` and calls
  // OnPIIDetected() when it's done.
  void OnAllGetRoutesDone(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container);

  // Merges `detected_pii` into `pii_map_` and runs
  // `on_data_collected_callback`.
  void OnPIIDetected(DataCollectorDoneCallback on_data_collected_callback,
                     PIIMap detected_pii);

  // Writes the contents of `network_routes_detected` into "network_routes.txt"
  // file in `target_directory`.
  void OnPIIRedacted(base::FilePath target_directory,
                     DataCollectorDoneCallback on_exported_callback,
                     std::vector<std::string> network_routes_redacted);

  // Runs `on_exported_callback` when file is written.
  void OnFilesWritten(DataCollectorDoneCallback on_exported_callback,
                      bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  std::vector<std::string> network_routes_;
  base::WeakPtrFactory<NetworkRoutesDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_ASH_NETWORK_ROUTES_DATA_COLLECTOR_H_
