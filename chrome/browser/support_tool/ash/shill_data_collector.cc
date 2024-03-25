// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/shill_data_collector.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/system_logs/shill_log_pii_identifiers.h"
#include "chrome/browser/support_tool/data_collector_utils.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/components/onc/onc_utils.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr char kNetworkDevices[] = "network_devices";
constexpr char kNetworkServices[] = "network_services";
constexpr char kServicePrefix[] = "/service/";
constexpr char kDevicePrefix[] = "/device/";

std::string GetString(const base::Value& value) {
  if (!value.is_string()) {
    LOG(ERROR) << "Bad string value: " << value;
    return std::string();
  }
  return value.GetString();
}

// Check the contents of `value` and returns true if its contents are empty. If
// `value` contains literal types like int, bool, it'll return false.
bool HasEmptyContents(const base::Value& value) {
  // Check non-literal types to see if they have empty contents.
  if (value.is_string())
    return value.GetString().empty();
  if (value.is_list())
    return value.GetList().empty();
  if (value.is_dict())
    return value.GetDict().empty();
  // The literal types can't be empty.
  return false;
}

constexpr char kMaskedString[] = "*** MASKED ***";

// Converts `shill_log` into std::string and detects PII sensitive data it
// contains. Returns the detected PII map.
PIIMap DetectPII(
    base::Value::Dict shill_log,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  PIIMap detected_pii;
  // Detect PII in `shill_log` and add the detected PII to `detected_pii`.
  std::string json;
  base::JSONWriter::WriteWithOptions(
      shill_log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  PIIMap pii_in_logs = redaction_tool->Detect(std::move(json));
  MergePIIMaps(detected_pii, pii_in_logs);
  return detected_pii;
}

// Converts `shill_log` into std::string and redacts PII sensitive data it
// contains.
std::string RedactAndKeepSelectedPII(
    const std::set<redaction::PIIType>& pii_types_to_keep,
    const base::Value::Dict& shill_log,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  redaction::RedactionTool* redaction_tool = redaction_tool_container->Get();
  std::string property_str;
  base::JSONWriter::WriteWithOptions(
      shill_log, base::JSONWriter::OPTIONS_PRETTY_PRINT, &property_str);
  property_str = redaction_tool->RedactAndKeepSelected(std::move(property_str),
                                                       pii_types_to_keep);
  return property_str;
}

// Creates `shill_properties.txt` at `target_directory` and writes the contents
// of `shill_property` into it. Returns true on success.
bool WriteOutputFiles(std::string shill_property,
                      base::FilePath target_directory) {
  return base::WriteFile(
      target_directory.Append(FILE_PATH_LITERAL("shill_properties.txt")),
      shill_property);
}

// Recursively traverses `dict`. Detects and adds PII listed in
// `system_logs::kShillPIIMaskedMap` in `dict` to `pii_map` if `scrub` is false;
// otherwise replace PII with `kMaskedString`.
void DetectOrScrubPIIInDictionary(
    base::Value::Dict& dict,
    bool scrub,
    std::set<redaction::PIIType>& pii_types_to_keep,
    PIIMap& pii_map) {
  for (auto entry : dict) {
    if (entry.second.is_dict()) {
      DetectOrScrubPIIInDictionary(entry.second.GetDict(), scrub,
                                   pii_types_to_keep, pii_map);
      continue;
    }
    if (!system_logs::kShillPIIMaskedMap.contains(entry.first))
      continue;
    // We don't add empty values to `pii_map` nor mask them because empty
    // values don't contain PII anyway.
    if (HasEmptyContents(entry.second))
      continue;
    if (scrub &&
        !base::Contains(pii_types_to_keep,
                        system_logs::kShillPIIMaskedMap.at(entry.first))) {
      entry.second = base::Value(kMaskedString);
      continue;
    }
    std::string value_as_string;
    base::JSONWriter::WriteWithOptions(
        entry.second, base::JSONWriter::OPTIONS_PRETTY_PRINT, &value_as_string);
    pii_map[system_logs::kShillPIIMaskedMap.at(entry.first)].emplace(
        value_as_string);
  }
}

}  // namespace

ShillDataCollector::ShillDataCollector() {
  shill_log_.Set(kNetworkDevices, base::Value::Dict());
  shill_log_.Set(kNetworkServices, base::Value::Dict());
  collector_err_ = {{"Device", {}}, {"Service", {}}, {"IPConfig", {}}};
}

ShillDataCollector::~ShillDataCollector() = default;

std::string ShillDataCollector::GetName() const {
  return "Shill";
}

std::string ShillDataCollector::GetDescription() const {
  return "Collects Shill data and exports the data into file named shill.";
}

const PIIMap& ShillDataCollector::GetDetectedPII() {
  return pii_map_;
}

void ShillDataCollector::CollectDataAndDetectPII(
    DataCollectorDoneCallback on_data_collected_callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_collector_done_callback_ = std::move(on_data_collected_callback);
  task_runner_for_redaction_tool_ = std::move(task_runner_for_redaction_tool);
  redaction_tool_container_ = std::move(redaction_tool_container);
  ash::ShillManagerClient::Get()->GetProperties(
      base::BindOnce(&ShillDataCollector::OnGetManagerProperties,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShillDataCollector::OnGetManagerProperties(
    std::optional<base::Value::Dict> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        "ShillDataCollector: ManagerPropertiesCallback failed"};
    std::move(data_collector_done_callback_).Run(/*error=*/error);
    return;
  }

  // Records how many entries are pending to be processed. Adds 1 to guard
  // against the case where `num_entries_left_` drops to 0 before all entries
  // are retrieved.
  num_entries_left_ = 1;
  const base::Value::List* devices = result->FindList(shill::kDevicesProperty);
  if (devices) {
    for (const base::Value& device : *devices) {
      std::string path = GetString(device);
      if (path.empty())
        continue;
      ++num_entries_left_;
      ash::ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillDataCollector::OnGetDevice,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  const base::Value::List* services =
      result->FindList(shill::kServicesProperty);
  if (services) {
    for (const base::Value& service : *services) {
      std::string path = GetString(service);
      if (path.empty())
        continue;
      ++num_entries_left_;
      ash::ShillServiceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillDataCollector::OnGetService,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }
  // Removes the protective "1" so that `num_entries_left_` can become 0.
  --num_entries_left_;
  CheckIfDone();
}

void ShillDataCollector::OnGetDevice(
    const std::string& device_path,
    std::optional<base::Value::Dict> properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!properties) {
    collector_err_["Device"].emplace_back(device_path);
  } else {
    AddDeviceAndRequestIPConfigs(device_path, *properties);
  }
  --num_entries_left_;
  CheckIfDone();
}

void ShillDataCollector::AddDeviceAndRequestIPConfigs(
    const std::string& device_path,
    const base::Value::Dict& properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  shill_log_.FindDict(kNetworkDevices)
      ->Set(device_path, ExpandProperties(device_path, properties));

  const base::Value::List* ip_configs =
      properties.FindList(shill::kIPConfigsProperty);
  if (!ip_configs)
    return;

  for (const base::Value& ip_config : *ip_configs) {
    std::string ip_config_path = GetString(ip_config);
    if (ip_config_path.empty())
      continue;
    ++num_entries_left_;
    ash::ShillIPConfigClient::Get()->GetProperties(
        dbus::ObjectPath(ip_config_path),
        base::BindOnce(&ShillDataCollector::OnGetIPConfig,
                       weak_ptr_factory_.GetWeakPtr(), device_path,
                       ip_config_path));
  }
}

void ShillDataCollector::OnGetIPConfig(
    const std::string& device_path,
    const std::string& ip_config_path,
    std::optional<base::Value::Dict> properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!properties) {
    collector_err_["IPConfig"].emplace_back(
        base::StrCat({device_path, ": ", ip_config_path}));
  } else {
    AddIPConfig(device_path, ip_config_path, *properties);
  }
  --num_entries_left_;
  CheckIfDone();
}

void ShillDataCollector::AddIPConfig(const std::string& device_path,
                                     const std::string& ip_config_path,
                                     const base::Value::Dict& properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict& device =
      shill_log_.FindDict(kNetworkDevices)->Find(device_path)->GetDict();
  base::Value::Dict* ip_configs = device.FindDict(shill::kIPConfigsProperty);
  if (!ip_configs) {
    ip_configs =
        device.Set(shill::kIPConfigsProperty, base::Value::Dict())->GetIfDict();
  }
  DCHECK(ip_configs);
  ip_configs->Set(ip_config_path, ExpandProperties(ip_config_path, properties));
}

void ShillDataCollector::OnGetService(
    const std::string& service_path,
    std::optional<base::Value::Dict> properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!properties) {
    collector_err_["Service"].emplace_back(service_path);
  } else {
    shill_log_.FindDict(kNetworkServices)
        ->Set(service_path, ExpandProperties(service_path, properties.value()));
  }
  --num_entries_left_;
  CheckIfDone();
}

base::Value::Dict ShillDataCollector::ExpandProperties(
    const std::string& object_path,
    const base::Value::Dict& properties) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dict = properties.Clone();
  // Converts UIData from a string to a dictionary.
  std::string* ui_data = dict.FindString(shill::kUIDataProperty);
  if (ui_data) {
    std::optional<base::Value::Dict> ui_data_dict =
        chromeos::onc::ReadDictionaryFromJson(*ui_data);
    if (ui_data_dict.has_value()) {
      dict.Set(shill::kUIDataProperty, base::Value(std::move(*ui_data_dict)));
    }
  }

  if (base::StartsWith(object_path, kServicePrefix,
                       base::CompareCase::SENSITIVE)) {
    pii_map_[redaction::PIIType::kSSID].insert(
        *dict.FindString(shill::kNameProperty));
  } else if (base::StartsWith(object_path, kDevicePrefix,
                              base::CompareCase::SENSITIVE)) {
    pii_map_[redaction::PIIType::kSSID].insert(
        *dict.FindString(shill::kNameProperty));
    // Only detects "Address" in the top level Device dictionary, not globally
    // (which would mask IPConfigs which get anonymized separately).
    pii_map_[redaction::PIIType::kMACAddress].insert(
        *dict.FindString(shill::kAddressProperty));
  }
  std::set<redaction::PIIType> empty = {};
  DetectOrScrubPIIInDictionary(dict, /*scrub=*/false,
                               /*pii_types_to_keep=*/empty, pii_map_);
  return dict;
}

void ShillDataCollector::CheckIfDone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (num_entries_left_)
    return;
  task_runner_for_redaction_tool_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DetectPII, shill_log_.Clone(), redaction_tool_container_),
      base::BindOnce(&ShillDataCollector::OnPIIDetected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShillDataCollector::OnPIIDetected(PIIMap detected_pii) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& entry : detected_pii)
    pii_map_[entry.first].insert(entry.second.begin(), entry.second.end());
  // Generates error message, if any.
  std::string collector_errors;
  for (const auto& err : collector_err_) {
    if (err.second.size()) {
      base::StrAppend(&collector_errors,
                      {"Get ", err.first, " Properties Failed for : ",
                       base::JoinString(err.second, ", "), "\n"});
    }
  }
  if (collector_errors.size()) {
    SupportToolError error = {
        SupportToolErrorCode::kDataCollectorError,
        base::StrCat({"ShillDataCollector had errors collecting data: ",
                      collector_errors})};
    std::move(data_collector_done_callback_).Run(/*error=*/error);
  } else {
    std::move(data_collector_done_callback_).Run(/*error=*/std::nullopt);
  }
}

void ShillDataCollector::ExportCollectedDataWithPII(
    std::set<redaction::PIIType> pii_types_to_keep,
    base::FilePath target_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
    scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
    DataCollectorDoneCallback on_exported_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only masks shill::kNameProperty in the top levels of devices and services.
  if (!pii_types_to_keep.count(redaction::PIIType::kSSID)) {
    for (auto entry : *shill_log_.FindDict(kNetworkServices)) {
      std::string log_name = ash::NetworkPathId(entry.first);  // Not PII
      entry.second.GetDict().Set(shill::kNameProperty, log_name);
    }
    for (auto entry : *shill_log_.FindDict(kNetworkDevices))
      entry.second.GetDict().Set(shill::kNameProperty, kMaskedString);
  }
  // Masks PII listed in `system_logs::kShillPIIMaskedMap`.
  DetectOrScrubPIIInDictionary(shill_log_, /*scrub=*/true, pii_types_to_keep,
                               pii_map_);
  // Runs RedactionTool to further remove PII.
  task_runner_for_redaction_tool->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RedactAndKeepSelectedPII, pii_types_to_keep,
                     shill_log_.Clone(), redaction_tool_container),
      base::BindOnce(&ShillDataCollector::OnPIIRedacted,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void ShillDataCollector::OnPIIRedacted(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    std::string shill_property) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteOutputFiles, std::move(shill_property),
                     target_directory),
      base::BindOnce(&ShillDataCollector::OnFilesWritten,
                     weak_ptr_factory_.GetWeakPtr(), target_directory,
                     std::move(on_exported_callback)));
}

void ShillDataCollector::OnFilesWritten(
    base::FilePath target_directory,
    DataCollectorDoneCallback on_exported_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    SupportToolError error = {SupportToolErrorCode::kDataCollectorError,
                              "ShillDataCollector failed on data export."};
    std::move(on_exported_callback).Run(error);
    return;
  }
  shill_log_.clear();
  std::move(on_exported_callback).Run(/*error=*/std::nullopt);
}
