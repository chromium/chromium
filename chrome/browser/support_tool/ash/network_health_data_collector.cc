// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/network_health_data_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/system_logs/network_health_source.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Regex pattern has three matching groups: first one is for the skipped input
// that doesn't contain any network names and second one is for the matched
// network name and the third one is mathced network GUID.
constexpr char kRegexPattern[] =
    "(?s)(.*?)Name: (?-s)(.+)\\nGUID: (?-s)(.+)\\n";

// Looks for network names and GUID in `network_health_data` and adds the found
// ones to `out_pii_map` under redaction::PIIType::kStableIdentifier category.
void FindNetworkNamesAndAddToPIIMap(const std::string& network_health_data,
                                    PIIMap& out_pii_map) {
  // `network_health_data` stores every component in a new line. Network names
  // are stored in "Name: <name>\n" format in `network_health_data`. The GUID is
  // put on the line after network name and is in format "GUID: <guid>\n".
  re2::RE2 regex_pattern(kRegexPattern);
  std::string_view input(network_health_data);

  std::string_view skipped_part;
  std::string_view matched_network_name;
  std::string_view matched_guid;

  while (re2::RE2::Consume(&input, regex_pattern, &skipped_part,
                           &matched_network_name, &matched_guid)) {
    if (matched_network_name != "N/A")
      out_pii_map[redaction::PIIType::kStableIdentifier].emplace(
          matched_network_name);
    if (matched_guid != "N/A")
      out_pii_map[redaction::PIIType::kStableIdentifier].emplace(matched_guid);
  }
}

// Finds the network names and GUIDs in the `network_health_data` and replaces
// them with anonymized network IDs.
std::string RedactNetworkNames(const std::string& network_health_data) {
  // `network_health_data` stores every component in a new line. Network names
  // are stored in "Name: <name>\n" format in `network_health_data`. The GUID is
  // put on the line after network name and is in format "GUID: <guid>\n".
  re2::RE2 regex_pattern(kRegexPattern);
  std::string_view input(network_health_data);

  std::string_view skipped_part;
  std::string_view matched_network_name;
  std::string_view matched_guid;
  std::string redacted;

  while (re2::RE2::Consume(&input, regex_pattern, &skipped_part,
                           &matched_network_name, &matched_guid)) {
    redacted.append(skipped_part);
    if (matched_network_name == "N/A" || matched_network_name.empty() ||
        matched_guid == "N/A" || matched_guid.empty()) {
      redacted += "Name: N/A\nGUID: N/A\n";
      continue;
    }
    std::string replacement = ash::NetworkGuidId(std::string(matched_guid));
    redacted += base::StringPrintf("Name: %s\nGUID: %s\n", replacement.c_str(),
                                   replacement.c_str());
  }
  // Append the rest of the input to `redacted`. Only the unmatched last part
  // will be present in the `input` as we're using Consume() function.
  redacted.append(input);
  return redacted;
}

}  // namespace

NetworkHealthDataCollector::NetworkHealthDataCollector()
    : SystemLogSourceDataCollectorAdaptor(
          "Fetches network health and diagnostics information.",
          std::make_unique<system_logs::NetworkHealthSource>(
              /*scrub=*/false,
              /*include_guid_when_not_scrub=*/true)) {}

NetworkHealthDataCollector::~NetworkHealthDataCollector() = default;

void NetworkHealthDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  SystemLogSourceDataCollectorAdaptor::CollectDataAndDetectPII(
      base::BindOnce(&NetworkHealthDataCollector::
                         OnSystemLogSourceDataCollectorAdaptorCollectedData,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_data_collected_callback)),
      task_runner_for_redaction_tool, redaction_tool_container);
}

void NetworkHealthDataCollector::
    OnSystemLogSourceDataCollectorAdaptorCollectedData(
        DataCollectorDoneCallback on_data_collected_callback,
        std::optional<SupportToolError> error) {
  // `system_logs::kNetworkHealthSnapshotEntry` contains network names and they
  // should be detected specially since
  // `SystemLogSourceDataCollectorAdaptor::CollectDataAndDetectPII()` can't
  // detect them.
  std::string network_health_snapshot =
      system_logs_response_->at(system_logs::kNetworkHealthSnapshotEntry);
  FindNetworkNamesAndAddToPIIMap(network_health_snapshot, pii_map_);
  std::move(on_data_collected_callback).Run(error);
}

void NetworkHealthDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  if (!base::Contains(pii_types_to_keep,
                      redaction::PIIType::kStableIdentifier)) {
    // `system_logs::kNetworkHealthSnapshotEntry` contains network names and
    // they should be anonymised specially since
    // `SystemLogSourceDataCollectorAdaptor::ExportCollectedDataWithPII()` can't
    // remove them.
    std::string network_health_snapshot =
        system_logs_response_->at(system_logs::kNetworkHealthSnapshotEntry);
    system_logs_response_->at(system_logs::kNetworkHealthSnapshotEntry) =
        RedactNetworkNames(network_health_snapshot);
  }
  SystemLogSourceDataCollectorAdaptor::ExportCollectedDataWithPII(
      pii_types_to_keep, target_directory, task_runner_for_redaction_tool,
      redaction_tool_container, std::move(on_exported_callback));
}
