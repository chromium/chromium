// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/system_logs_data_collector.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/data_collector_utils.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace {

// Return the name debugd sends the logs in the `log_file_name` file
// in the file system.
base::FilePath GetDebugdPathOfLog(const base::FilePath& log_file_name) {
  // Most of the logs in debugd are stored with their file's base name. However,
  // there are some exceptions. We map the log file's base name in file system
  // and their name in debugd for these exceptional cases.
  static constexpr auto kDebugdLogNames =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{"arc.log", "cheets_log"},
           {"chrome", "chrome_system_log"},
           {"chrome.PREVIOUS", "chrome_system_log.PREVIOUS"},
           {"memd.clip", "memd clips"},
           {"net.log", "netlog"},
           {"messages", "syslog"},
           {"ui.LATEST", "ui_log"},
           {"debug_vboot_noisy.log", "verified boot"}});
  auto log_name = kDebugdLogNames.find(log_file_name.value());
  return log_name == kDebugdLogNames.end() ? log_file_name
                                           : base::FilePath(log_name->second);
}

// Filters the logs in `logs` and returns the map containing only the logs
// specified in `requested_logs`. Returns the `logs` without any filtering if
// `requested_logs` is empty.
std::map<std::string, std::string> GetOnlyRequestedLogs(
    const std::map<std::string, std::string>& logs,
    const std::set<base::FilePath>& requested_logs) {
  // Return all logs without filtering if `requested_logs` is not specified.
  if (requested_logs.empty())
    return std::move(logs);

  std::map<std::string, std::string> filtered_logs;
  for (const base::FilePath& requested_log : requested_logs) {
    base::FilePath requested_log_name = GetDebugdPathOfLog(requested_log);
    // We check if `logs` contain `requested_log_name` first.
    auto search_result = logs.find(requested_log_name.value());
    if (search_result != logs.end()) {
      filtered_logs.emplace(search_result->first, search_result->second);
      continue;
    }
    // If `logs` doesn't contain `requested_log_name`, we also check if it
    // contains the version that doesn't have extension since debugd may remove
    // extension of log file's name when returning the logs.
    search_result = logs.find(requested_log_name.RemoveExtension().value());
    if (search_result != logs.end()) {
      filtered_logs.emplace(search_result->first, search_result->second);
      continue;
    }
  }
  return filtered_logs;
}

// Detects PII sensitive data that `system_logs` contains and returns
// the detected PII map.
PIIMap DetectPII(
    std::map<std::string, std::string> system_logs,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in all entries in `system_logs` and add the detected
  // PII to `detected_pii`.
  for (const auto& entry : system_logs) {
    PIIMap pii_in_logs = redaction_tool->Detect(entry.second);
    MergePIIMaps(detected_pii, pii_in_logs);
  }
  return detected_pii;
}

// Redacts PII from `system_logs` except `pii_types_to_keep` and returns the map
// containing redacted logs.
std::map<std::string, std::string> RedactPII(
    std::map<std::string, std::string> system_logs,
    const std::set<redaction::PIIType>& pii_types_to_keep,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  for (auto& [log_name, log_contents] : system_logs) {
    log_contents =
        redaction_tool->RedactAndKeepSelected(log_contents, pii_types_to_keep);
  }
  return system_logs;
}

// Opens a "var_log_files" file under `target_directory` and writes
// `system_logs` into it.
bool WriteSystemLogFiles(std::map<std::string, std::string> system_logs,
                         base::FilePath target_directory) {
  base::FilePath target_path =
      target_directory.Append(FILE_PATH_LITERAL("var_log_files"));
  if (!base::CreateDirectory(target_path))
    return false;
  bool success = true;
  for (const auto& [log_name, log_contents] : system_logs) {
    if (!base::WriteFile(target_path.AppendASCII(log_name), log_contents))
      success = false;
  }
  return success;
}

}  // namespace

SystemLogsDataCollector::SystemLogsDataCollector(
    std::set<base::FilePath> requested_logs)
    : requested_logs_(requested_logs) {}
SystemLogsDataCollector::~SystemLogsDataCollector() = default;

std::string SystemLogsDataCollector::GetName() const {
  return "System Logs Data Collector";
}

std::string SystemLogsDataCollector::GetDescription() const {
  return "Collect various system logs under /var/log in ChromeOS devices";
}

const PIIMap& SystemLogsDataCollector::GetDetectedPII() {
  return pii_map_;
}

void SystemLogsDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  // We will only request /var/log files.
  const std::vector<debugd::FeedbackLogType> included_log_types = {
      debugd::FeedbackLogType::VAR_LOG_FILES};

  // `debugd_client` will run the callback on original thread (see
  // dbus/object_proxy.h for more details).
  debugd_client->GetFeedbackLogs(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user ? user->GetAccountId() : EmptyAccountId()),
      included_log_types,
      base::BindOnce(&SystemLogsDataCollector::OnGetFeedbackLogs,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback),
                     task_runner_for_redaction_tool, redaction_tool_container));
}

void SystemLogsDataCollector::OnGetFeedbackLogs(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    bool success,
    const std::map<std::string, std::string>& logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  system_logs_ = GetOnlyRequestedLogs(logs, requested_logs_);

  // If `system_logs_` are empty, it means the data collector couldn't get any
  // of the requested logs. Return the error message.
  if (system_logs_.empty()) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "SystemLogsDataCollector couldn't retrieve requested logs."};
    std::move(on_data_collected_callback).Run(error);
    return;
  }

  // There might be some logs missing if `success` is not true. Document it in
  // error message even though some of the logs could be retrieved successfully.
  std::optional<SupportToolError> error =
      success ? std::nullopt
              : std::make_optional(
                    SupportToolError(SupportToolErrorCode::kDataCollectorError,
                                     "SystemLogsDataCollector got error from "
                                     "debugd when requesting logs."));

  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, system_logs_, redaction_tool_container),
      base::BindOnce(&SystemLogsDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback), error));
}

void SystemLogsDataCollector::OnPIIDetected(
    DataCollectorDoneCallback on_data_collected_callback,
    std::optional<SupportToolError> error,
    PIIMap detected_pii) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pii_map_ = detected_pii;
  std::move(on_data_collected_callback).Run(error);
}

void SystemLogsDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactPII, std::move(system_logs_), pii_types_to_keep,
                     redaction_tool_container),
      base::BindOnce(&SystemLogsDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void SystemLogsDataCollector::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::map<std::string, std::string> redacted_logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteSystemLogFiles, redacted_logs, target_directory),
      base::BindOnce(&SystemLogsDataCollector::OnFilesWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void SystemLogsDataCollector::OnFilesWritten(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  if (!success) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "SystemLogsDataCollector failed to write system log files."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}
