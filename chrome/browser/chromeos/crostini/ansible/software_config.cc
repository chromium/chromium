// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/software_config.h"

#include <utility>

#include "base/json/json_reader.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_util.h"

namespace crostini {

namespace {

// Key and field names that we expect in JSON config input.
const char kConfigVersionKey[] = "version";
const char kConfigKeysKey[] = "keys";
const char kConfigSourcesKey[] = "sources";
const char kConfigPackagesKey[] = "packages";

const char kConfigUrlField[] = "url";
const char kConfigLineField[] = "line";
const char kConfigNameField[] = "name";

// Initial schema version.
const int kConfigCurrentVersion = 1;

// Utility function to save string field values from a list of dictionaries
// to a list of strings.
base::Optional<std::vector<std::string>> CopyListFieldValues(
    const base::Value* list,
    const base::StringPiece field) {
  if (list == nullptr || !list->is_list())
    return base::nullopt;

  std::vector<std::string> result;

  for (const auto& item : list->GetList()) {
    if (!item.is_dict())
      return base::nullopt;
    const std::string* field_value = item.FindStringKey(field);
    if (field_value == nullptr)
      return base::nullopt;
    result.push_back(*field_value);
  }

  return result;
}

}  // namespace

SoftwareConfig::SoftwareConfig() = default;
SoftwareConfig::SoftwareConfig(SoftwareConfig&&) = default;
SoftwareConfig::~SoftwareConfig() = default;

base::Optional<SoftwareConfig> SoftwareConfig::FromJson(
    const std::string& config_json) {
  // Empty config is considered valid (initial value, "empty state").
  if (config_json.empty())
    return SoftwareConfig();

  // If not empty, ensure config is a valid JSON representing a dictionary.
  const auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
      config_json, base::JSON_PARSE_RFC);
  if (!parse_result.value.has_value()) {
    LOG(ERROR) << "JSON input is malformed: " << parse_result.error_message;
    return base::nullopt;
  }

  if (!parse_result.value->is_dict()) {
    LOG(ERROR) << "Parsed JSON input is not a dictionary.";
    return base::nullopt;
  }

  // Ensure that we do not attempt to parse config in an incompatible format.
  const base::Optional<int> format_version_field =
      parse_result.value->FindIntKey(kConfigVersionKey);

  if (!format_version_field.has_value() ||
      format_version_field != kConfigCurrentVersion) {
    LOG(ERROR) << "Ansible config format version is missing or not supported "
                  "by this version of Chromium.";
    return base::nullopt;
  }

  // Extract and store config data.
  const base::Value* gpg_keys_field =
      parse_result.value->FindListKey(kConfigKeysKey);
  const base::Value* sources_field =
      parse_result.value->FindListKey(kConfigSourcesKey);
  const base::Value* packages_field =
      parse_result.value->FindListKey(kConfigPackagesKey);

  auto parsed_key_urls = CopyListFieldValues(gpg_keys_field, kConfigUrlField);
  auto parsed_source_lines =
      CopyListFieldValues(sources_field, kConfigLineField);
  auto parsed_package_names =
      CopyListFieldValues(packages_field, kConfigNameField);

  if (!parsed_key_urls.has_value() || !parsed_source_lines.has_value() ||
      !parsed_package_names.has_value()) {
    LOG(ERROR) << "Ansible config is missing required data.";
    return base::nullopt;
  }

  SoftwareConfig config;
  config.key_urls_ = std::move(parsed_key_urls.value());
  config.source_lines_ = std::move(parsed_source_lines.value());
  config.package_names_ = std::move(parsed_package_names.value());
  return config;
}

void SoftwareConfig::SetKeysForTesting(std::vector<std::string> key_urls) {
  key_urls_ = std::move(key_urls);
}

void SoftwareConfig::SetSourcesForTesting(
    std::vector<std::string> source_lines) {
  source_lines_ = std::move(source_lines);
}

void SoftwareConfig::SetPackagesForTesting(
    std::vector<std::string> package_names) {
  package_names_ = std::move(package_names);
}

}  // namespace crostini
