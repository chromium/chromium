// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/system_state_data_collector.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace {

// List of debugd entries to exclude from the results.
constexpr std::array<const char*, 2> kExcludeList = {
    // Network devices and services are collected by `ShillDataCollector` so we
    // exclude them from the output.
    "network-devices",
    "network-services",
};

// Adds the contents of `map_to_merge` into `target_map` and returns a set of
// keys in `map_to_merge`.
std::set<redaction::PIIType> MergePIIMapsAndGetPIITypes(PIIMap& target_map,
                                                       PIIMap& map_to_merge) {
  std::set<redaction::PIIType> keys;
  for (auto& pii_data : map_to_merge) {
    target_map[pii_data.first].insert(pii_data.second.begin(),
                                      pii_data.second.end());
    keys.insert(pii_data.first);
  }
  return keys;
}

// Creates the directory on `target_path` if it doesn't exist and writes entries
// in `system_logs` to file in `target_path`. Returns true on success.
bool WriteLogFiles(std::map<std::string, std::string> system_logs,
                   base::FilePath target_path) {
  if (!base::CreateDirectory(target_path))
    return false;
  bool success = true;
  for (auto [log_name, logs] : system_logs) {
    if (!base::WriteFile(target_path.Append(log_name).AddExtensionASCII(".txt"),
                         logs))
      success = false;
  }
  return success;
}

std::string GetErrorMessage(const std::vector<std::string>& errors) {
  return base::StringPrintf("SystemStateDataCollector had following errors: %s",
                            base::JoinString(errors, ", ").c_str());
}

}  // namespace

SystemStateDataCollector::SystemLog::SystemLog(
    std::string log,
    std::set<redaction::PIIType> detected_pii_types)
    : log(log), detected_pii_types(std::move(detected_pii_types)) {}
SystemStateDataCollector::SystemLog::~SystemLog() = default;
SystemStateDataCollector::SystemLog::SystemLog(const SystemLog& other) =
    default;

SystemStateDataCollector::SystemStateDataCollector() = default;
SystemStateDataCollector::~SystemStateDataCollector() = default;

const std::vector<std::string> SystemStateDataCollector::GetExtraLogNames() {
  const std::vector<std::string> kExtraLogs = {"netstat"};
  return kExtraLogs;
}

std::string SystemStateDataCollector::GetName() const {
  return "System State Data Collector";
}

std::string SystemStateDataCollector::GetDescription() const {
  return "Collect various system logs and reports for ChromeOS devices and "
         "writes them to them to separate files.";
}

const PIIMap& SystemStateDataCollector::GetDetectedPII() {
  return pii_map_;
}

void SystemStateDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_collector_done_callback_ = std::move(on_data_collected_callback);

  base::RepeatingClosure get_log_barrier_closure = base::BarrierClosure(
      GetExtraLogNames().size(),
      base::BindOnce(&SystemStateDataCollector::OnGotAllExtraLogs,
                     weak_ptr_factory_.GetWeakPtr(),
                     task_runner_for_redaction_tool, redaction_tool_container));

  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();

  for (const auto& extra_log : GetExtraLogNames()) {
    // `debugd_client` will run the callback on original thread (see
    // dbus/object_proxy.h for more details).
    debugd_client->GetLog(extra_log,
                          base::BindOnce(&SystemStateDataCollector::OnGetLog,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         get_log_barrier_closure, extra_log));
  }
}

void SystemStateDataCollector::OnGetLog(base::RepeatingClosure barrier_closure,
                                        std::string log_name,
                                        std::optional<std::string> log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!log) {
    get_log_errors_.push_back(base::StringPrintf(
        "Couldn't get '%s' logs from GetLog call to debugd", log_name.c_str()));
    std::move(barrier_closure).Run();
    return;
  }
  system_logs_.emplace(log_name, SystemLog(log.value(), {}));
  std::move(barrier_closure).Run();
}

void SystemStateDataCollector::OnGotAllExtraLogs(
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::DebugDaemonClient* debugd_client = ash::DebugDaemonClient::Get();

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  // The list of log types that we request from debugd.
  const std::vector<debugd::FeedbackLogType> included_log_types = {
      debugd::FeedbackLogType::ARC_BUG_REPORT,
      debugd::FeedbackLogType::CONNECTIVITY_REPORT,
      debugd::FeedbackLogType::VERBOSE_COMMAND_LOGS,
      debugd::FeedbackLogType::COMMAND_LOGS,
      debugd::FeedbackLogType::FEEDBACK_LOGS,
      debugd::FeedbackLogType::BLUETOOTH_BQR,
      debugd::FeedbackLogType::LSB_RELEASE_INFO,
      debugd::FeedbackLogType::PERF_DATA,
      debugd::FeedbackLogType::OS_RELEASE_INFO};

  // DBus operations on Chromium is run on UI thread (see
  // https://chromium.googlesource.com/chromiumos/docs/+/master/dbus_in_chrome.md#using-system-daemons_d_bus-services).
  // `debugd_client` will run the callback on original thread (see
  // dbus/object_proxy.h for more details).
  debugd_client->GetFeedbackLogs(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user ? user->GetAccountId() : EmptyAccountId()),
      included_log_types,
      base::BindOnce(&SystemStateDataCollector::OnGetFeedbackLogs,
                     weak_ptr_factory_.GetWeakPtr(),
                     task_runner_for_redaction_tool, redaction_tool_container));
}

void SystemStateDataCollector::OnGetFeedbackLogs(
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    bool success,
    const std::map<std::string, std::string>& logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& [log_name, log] : logs) {
    // Don't include `kExcludeList` in the output.
    if (base::Contains(kExcludeList, log_name))
      continue;
    system_logs_.emplace(log_name, SystemLog(log, {}));
  }

  if (!success)
    get_log_errors_.push_back(
        "Couldn't get logs from GetFeedbackLogs call to debugd");

  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SystemStateDataCollector::DetectPII, system_logs_,
                     redaction_tool_container),
      base::BindOnce(&SystemStateDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemStateDataCollector::OnPIIDetected(
    std::pair<PIIMap, std::map<std::string, SystemLog>> detection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pii_map_ = std::move(detection_result.first);
  system_logs_ = std::move(detection_result.second);
  if (!get_log_errors_.empty()) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              GetErrorMessage(get_log_errors_)};
    std::move(data_collector_done_callback_).Run(std::move(error));
    return;
  }
  std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
}

void SystemStateDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath target_path =
      target_directory.Append(FILE_PATH_LITERAL("chromeos_system_state"));
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SystemStateDataCollector::RedactPII,
                     std::move(system_logs_), pii_types_to_keep,
                     redaction_tool_container),
      base::BindOnce(&SystemStateDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_path,
                     std::move(on_exported_callback)));
}

void SystemStateDataCollector::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::map<std::string, std::string> redacted_logs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteLogFiles, redacted_logs, target_directory),
      base::BindOnce(&SystemStateDataCollector::OnFilesWritten,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_exported_callback)));
}

void SystemStateDataCollector::OnFilesWritten(
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  if (!success) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "SystemStateDataCollector failed on exporting system reports."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}

// static
std::pair<PIIMap, std::map<std::string, SystemStateDataCollector::SystemLog>>
SystemStateDataCollector::DetectPII(
    std::map<std::string, SystemStateDataCollector::SystemLog> system_logs,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in all entries in `logs` and add the detected
  // PII to `detected_pii`.
  for (auto& [log_name, system_log] : system_logs) {
    PIIMap pii_in_logs = redaction_tool->Detect(system_log.log);
    system_log.detected_pii_types =
        MergePIIMapsAndGetPIITypes(detected_pii, pii_in_logs);
  }
  return {std::move(detected_pii), std::move(system_logs)};
}

std::map<std::string, std::string> SystemStateDataCollector::RedactPII(
    std::map<std::string, SystemStateDataCollector::SystemLog> system_logs,
    std::set<redaction::PIIType> pii_types_to_keep,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  std::map<std::string, std::string> redacted_logs;
  for (auto [log_name, system_log] : system_logs) {
    redacted_logs[log_name] =
        std::includes(pii_types_to_keep.begin(), pii_types_to_keep.end(),
                      system_log.detected_pii_types.begin(),
                      system_log.detected_pii_types.end())
            ? system_log.log
            : redaction_tool->RedactAndKeepSelected(system_log.log,
                                                    pii_types_to_keep);
  }
  return redacted_logs;
}
