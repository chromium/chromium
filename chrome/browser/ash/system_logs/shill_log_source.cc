// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_source.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/system_logs/shill_log_pii_identifiers.h"
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

std::string GetString(const base::Value* value) {
  if (!value)
    return std::string();
  if (!value->is_string()) {
    LOG(ERROR) << "Bad string value: " << *value;
    return std::string();
  }
  return value->GetString();
}

constexpr char kMaskedString[] = "*** MASKED ***";

// Recursively scrubs dictionaries, masking any values in
// system_logs::MakeFixedFlatMap.
void ScrubDictionary(base::Value::Dict& dict) {
  for (auto entry : dict) {
    base::Value& value = entry.second;
    if (value.is_dict()) {
      ScrubDictionary(entry.second.GetDict());
    } else if (base::Contains(system_logs::kShillPIIMaskedMap, entry.first) &&
               system_logs::kShillPIIMaskedMap.at(entry.first) !=
                   redaction::PIIType::kNone &&
               (!value.is_string() || !value.GetString().empty())) {
      entry.second = base::Value(kMaskedString);
    }
  }
}

}  // namespace

namespace system_logs {

ShillLogSource::ShillLogSource(bool scrub)
    : SystemLogsSource("Shill"), scrub_(scrub) {}

ShillLogSource::~ShillLogSource() = default;

void ShillLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);

  ash::ShillManagerClient::Get()->GetProperties(base::BindOnce(
      &ShillLogSource::OnGetManagerProperties, weak_ptr_factory_.GetWeakPtr()));
}

void ShillLogSource::OnGetManagerProperties(
    std::optional<base::Value::Dict> result) {
  if (!result) {
    LOG(ERROR) << "ManagerPropertiesCallback Failed";
    std::move(callback_).Run(std::make_unique<SystemLogsResponse>());
    return;
  }

  const base::Value::List* devices = result->FindList(shill::kDevicesProperty);
  if (devices) {
    for (const base::Value& device : *devices) {
      std::string path = GetString(&device);
      if (path.empty())
        continue;
      device_paths_.insert(path);
      ash::ShillDeviceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillLogSource::OnGetDevice,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  const base::Value::List* services =
      result->FindList(shill::kServicesProperty);
  if (services) {
    for (const base::Value& service : *services) {
      std::string path = GetString(&service);
      if (path.empty())
        continue;
      service_paths_.insert(path);
      ash::ShillServiceClient::Get()->GetProperties(
          dbus::ObjectPath(path),
          base::BindOnce(&ShillLogSource::OnGetService,
                         weak_ptr_factory_.GetWeakPtr(), path));
    }
  }

  CheckIfDone();
}

void ShillLogSource::OnGetDevice(const std::string& device_path,
                                 std::optional<base::Value::Dict> properties) {
  if (!properties) {
    LOG(ERROR) << "Get Device Properties Failed for : " << device_path;
  } else {
    AddDeviceAndRequestIPConfigs(device_path, *properties);
  }
  device_paths_.erase(device_path);
  CheckIfDone();
}

void ShillLogSource::AddDeviceAndRequestIPConfigs(
    const std::string& device_path,
    const base::Value::Dict& properties) {
  base::Value* device = devices_.Set(
      device_path, ScrubAndExpandProperties(device_path, properties));

  const base::Value::List* ip_configs =
      properties.FindList(shill::kIPConfigsProperty);
  if (!ip_configs) {
    return;
  }

  for (const base::Value& ip_config : *ip_configs) {
    std::string ip_config_path = GetString(&ip_config);
    if (ip_config_path.empty()) {
      continue;
    }
    ip_config_paths_.insert(ip_config_path);
    ash::ShillIPConfigClient::Get()->GetProperties(
        dbus::ObjectPath(ip_config_path),
        base::BindOnce(&ShillLogSource::OnGetIPConfig,
                       weak_ptr_factory_.GetWeakPtr(), device_path,
                       ip_config_path));
  }
  if (!ip_config_paths_.empty()) {
    device->GetDict().Set(shill::kIPConfigsProperty, base::Value::Dict{});
  }
}

void ShillLogSource::OnGetIPConfig(
    const std::string& device_path,
    const std::string& ip_config_path,
    std::optional<base::Value::Dict> properties) {
  if (!properties) {
    LOG(ERROR) << "Get IPConfig Properties Failed for : " << device_path << ": "
               << ip_config_path;
  } else {
    AddIPConfig(device_path, ip_config_path, *properties);
  }
  // Erase a single matching entry.
  ip_config_paths_.erase(ip_config_paths_.find(ip_config_path));
  CheckIfDone();
}

void ShillLogSource::AddIPConfig(const std::string& device_path,
                                 const std::string& ip_config_path,
                                 const base::Value::Dict& properties) {
  base::Value::Dict* device = devices_.FindDict(device_path);
  DCHECK(device);
  base::Value::Dict* ip_configs = device->FindDict(shill::kIPConfigsProperty);
  DCHECK(ip_configs);
  ip_configs->Set(ip_config_path,
                  ScrubAndExpandProperties(ip_config_path, properties));
}

void ShillLogSource::OnGetService(const std::string& service_path,
                                  std::optional<base::Value::Dict> properties) {
  if (!properties) {
    LOG(ERROR) << "Get Service Properties Failed for : " << service_path;
  } else {
    services_.Set(service_path,
                  ScrubAndExpandProperties(service_path, properties.value()));
  }
  service_paths_.erase(service_path);
  CheckIfDone();
}

base::Value::Dict ShillLogSource::ScrubAndExpandProperties(
    const std::string& object_path,
    const base::Value::Dict& properties) {
  base::Value::Dict dict = properties.Clone();

  // Convert UIData from a string to a dictionary.
  std::string* ui_data = dict.FindString(shill::kUIDataProperty);
  if (ui_data) {
    std::optional<base::Value::Dict> ui_data_dict =
        chromeos::onc::ReadDictionaryFromJson(*ui_data);
    if (ui_data_dict.has_value()) {
      dict.Set(shill::kUIDataProperty, std::move(*ui_data_dict));
    }
  }

  if (!scrub_)
    return dict;

  if (base::StartsWith(object_path, kServicePrefix,
                       base::CompareCase::SENSITIVE)) {
    std::string log_name = ash::NetworkPathId(object_path);  // Not PII
    dict.Set(shill::kNameProperty, log_name);
  } else if (base::StartsWith(object_path, kDevicePrefix,
                              base::CompareCase::SENSITIVE)) {
    dict.Set(shill::kNameProperty, kMaskedString);
    // Only mask "Address" in the top level Device dictionary, not globally
    // (which would mask IPConfigs which get anonymized separately).
    if (dict.contains(shill::kAddressProperty)) {
      dict.Set(shill::kAddressProperty, kMaskedString);
    }
  }

  ScrubDictionary(dict);
  return dict;
}

void ShillLogSource::CheckIfDone() {
  if (!device_paths_.empty() || !ip_config_paths_.empty() ||
      !service_paths_.empty()) {
    return;
  }

  std::map<std::string, std::string> response;
  std::string json;
  base::JSONWriter::WriteWithOptions(
      devices_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  response[kNetworkDevices] = std::move(json);
  base::JSONWriter::WriteWithOptions(
      services_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  response[kNetworkServices] = std::move(json);

  // Clear |devices_| and |services_|.
  devices_.clear();
  services_.clear();

  std::move(callback_).Run(
      std::make_unique<SystemLogsResponse>(std::move(response)));
}

}  // namespace system_logs
