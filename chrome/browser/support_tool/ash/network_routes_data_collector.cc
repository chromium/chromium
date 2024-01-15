// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/network_routes_data_collector.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/data_collector_utils.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"

namespace {

// Detects PII sensitive data that `network_routes` contains and returns
// the detected PII map.
PIIMap DetectPII(
    std::vector<std::string> network_routes,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in all entries in `network_routes` and add the detected
  // PII to `detected_pii`.
  for (const auto& entry : network_routes) {
    PIIMap pii_in_logs = redaction_tool->Detect(entry);
    MergePIIMaps(detected_pii, pii_in_logs);
  }
  return detected_pii;
}

// Redacts PII sensitive data in `network_routes` except
// `pii_types_to_keep` and replaces the values in `network_routes` with
// redacted versions. Returns the modified version of `network_routes`.
std::vector<std::string> RedactAndKeepSelectedPII(
    const std::set<redaction::PIIType>& pii_types_to_keep,
    std::vector<std::string> network_routes,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  for (auto & network_route : network_routes) {
    network_route = redaction_tool->RedactAndKeepSelected(
        network_route, pii_types_to_keep);
  }
  return network_routes;
}

// Writes entries in `network_routes` to file in `target_path`. Returns true on
// success.
bool WriteOutputFile(std::vector<std::string> network_routes,
                     base::FilePath target_path) {
  return base::WriteFile(target_path, base::JoinString(network_routes, "\n"));
}
}  // namespace

NetworkRoutesDataCollector::NetworkRoutesDataCollector() = default;
NetworkRoutesDataCollector::~NetworkRoutesDataCollector() = default;

std::string NetworkRoutesDataCollector::GetName() const {
  return "Network Routes";
}

std::string NetworkRoutesDataCollector::GetDescription() const {
  return "Collects network routing tables data for both IPv4 and IPv6 and "
         "writes it into \"network_routes\" file.";
}

const PIIMap& NetworkRoutesDataCollector::GetDetectedPII() {
  return pii_map_;
}

void NetworkRoutesDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();
  // We will call DebugDaemonClient::GetRoutes twice to get IPv4 and IPv6 routes
  // in separate calls.
  size_t get_routes_calls = 2;
  base::RepeatingClosure get_routes_barrier_closure = base::BarrierClosure(
      get_routes_calls,
      base::BindOnce(&NetworkRoutesDataCollector::OnAllGetRoutesDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback),
                     task_runner_for_redaction_tool, redaction_tool_container));
  debugd_client->GetRoutes(
      /*numeric=*/true, /*ipv6=*/false, /*all_tables=*/true,
      base::BindOnce(&NetworkRoutesDataCollector::OnGetRoutes,
                     weak_ptr_factory_.GetWeakPtr(),
                     get_routes_barrier_closure));
  debugd_client->GetRoutes(
      /*numeric=*/true, /*ipv6=*/true, /*all_tables=*/true,
      base::BindOnce(&NetworkRoutesDataCollector::OnGetRoutes,
                     weak_ptr_factory_.GetWeakPtr(),
                     get_routes_barrier_closure));
}

void NetworkRoutesDataCollector::OnGetRoutes(
    base::RepeatingClosure barrier_closure,
    std::optional<std::vector<std::string>> routes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (routes) {
    network_routes_.insert(network_routes_.end(), routes.value().begin(),
                           routes.value().end());
  } else {
    LOG(ERROR) << "Couldn't get network routes information";
  }
  std::move(barrier_closure).Run();
}

void NetworkRoutesDataCollector::OnAllGetRoutesDone(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, network_routes_, redaction_tool_container),
      base::BindOnce(&NetworkRoutesDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)));
}

void NetworkRoutesDataCollector::OnPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    PIIMap detected_pii) {
  MergePIIMaps(pii_map_, detected_pii);
  std::move(on_data_collected_callback).Run(/*error=*/std::nullopt);
}

void NetworkRoutesDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactAndKeepSelectedPII, pii_types_to_keep,
                     network_routes_, redaction_tool_container),
      base::BindOnce(&NetworkRoutesDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void NetworkRoutesDataCollector::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::vector<std::string> network_routes_redacted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath target_path =
      target_directory.Append(FILE_PATH_LITERAL("network_routes"))
          .AddExtension(FILE_PATH_LITERAL(".txt"));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFile, network_routes_redacted, target_path),
      base::BindOnce(&NetworkRoutesDataCollector::OnFilesWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void NetworkRoutesDataCollector::OnFilesWritten(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  if (!success) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "Failed on exporting network routes data."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}
