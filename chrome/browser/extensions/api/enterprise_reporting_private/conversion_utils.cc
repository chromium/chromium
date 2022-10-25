// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/conversion_utils.h"

#include <algorithm>

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/files/file_path.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "extensions/browser/extension_function.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/sys_string_conversions.h"
#include "components/device_signals/core/common/win/win_types.h"
#endif  // BUILDFLAG(IS_WIN)

using SignalCollectionError = device_signals::SignalCollectionError;
using PresenceValue = device_signals::PresenceValue;

namespace extensions {

namespace {

constexpr size_t kGeneralSignalUpperLimit = 128U;

absl::optional<ParsedSignalsError> TryParseError(
    const device_signals::SignalsAggregationResponse& response,
    const absl::optional<device_signals::BaseSignalResponse>& bundle) {
  absl::optional<std::string> error_string;
  if (response.top_level_error) {
    return ParsedSignalsError{response.top_level_error.value(),
                              /*is_top_level_error=*/true};
  }

  if (!bundle) {
    return ParsedSignalsError{SignalCollectionError::kMissingBundle,
                              /*is_top_level_error=*/false};
  }

  if (bundle->collection_error) {
    return ParsedSignalsError{bundle->collection_error.value(),
                              /*is_top_level_error=*/false};
  }

  return absl::nullopt;
}

api::enterprise_reporting_private::PresenceValue ConvertPresenceValue(
    PresenceValue presence) {
  switch (presence) {
    case PresenceValue::kUnspecified:
      return api::enterprise_reporting_private::PRESENCE_VALUE_UNSPECIFIED;
    case PresenceValue::kAccessDenied:
      return api::enterprise_reporting_private::PRESENCE_VALUE_ACCESS_DENIED;
    case PresenceValue::kNotFound:
      return api::enterprise_reporting_private::PRESENCE_VALUE_NOT_FOUND;
    case PresenceValue::kFound:
      return api::enterprise_reporting_private::PRESENCE_VALUE_FOUND;
  }
}

std::string EncodeHash(const std::string& byte_string) {
  std::string encoded_string;
  base::Base64UrlEncode(byte_string, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);
  return encoded_string;
}

std::vector<std::string> EncodeHashes(
    const std::vector<std::string>& byte_strings) {
  std::vector<std::string> encoded_strings;
  const size_t upper_bound =
      std::min(kGeneralSignalUpperLimit, byte_strings.size());
  for (size_t i = 0; i < upper_bound; ++i) {
    encoded_strings.push_back(EncodeHash(byte_strings[i]));
  }
  return encoded_strings;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

absl::optional<device_signals::RegistryHive> ConvertHiveFromApi(
    api::enterprise_reporting_private::RegistryHive api_hive) {
  switch (api_hive) {
    case api::enterprise_reporting_private::REGISTRY_HIVE_HKEY_CLASSES_ROOT:
      return device_signals::RegistryHive::kHkeyClassesRoot;
    case api::enterprise_reporting_private::REGISTRY_HIVE_HKEY_LOCAL_MACHINE:
      return device_signals::RegistryHive::kHkeyLocalMachine;
    case api::enterprise_reporting_private::REGISTRY_HIVE_HKEY_CURRENT_USER:
      return device_signals::RegistryHive::kHkeyCurrentUser;
    case api::enterprise_reporting_private::REGISTRY_HIVE_NONE:
      return absl::nullopt;
  }
}

api::enterprise_reporting_private::RegistryHive ConvertHiveToApi(
    absl::optional<device_signals::RegistryHive> hive) {
  if (!hive) {
    return api::enterprise_reporting_private::REGISTRY_HIVE_NONE;
  }

  switch (hive.value()) {
    case device_signals::RegistryHive::kHkeyClassesRoot:
      return api::enterprise_reporting_private::REGISTRY_HIVE_HKEY_CLASSES_ROOT;
    case device_signals::RegistryHive::kHkeyLocalMachine:
      return api::enterprise_reporting_private::
          REGISTRY_HIVE_HKEY_LOCAL_MACHINE;
    case device_signals::RegistryHive::kHkeyCurrentUser:
      return api::enterprise_reporting_private::REGISTRY_HIVE_HKEY_CURRENT_USER;
  }
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

}  // namespace

std::vector<device_signals::GetFileSystemInfoOptions>
ConvertFileSystemInfoOptions(
    const std::vector<
        api::enterprise_reporting_private::GetFileSystemInfoOptions>&
        api_options) {
  std::vector<device_signals::GetFileSystemInfoOptions> converted_options;
  for (const auto& api_options_param : api_options) {
    device_signals::GetFileSystemInfoOptions converted_param;
    converted_param.file_path =
        base::FilePath::FromUTF8Unsafe(api_options_param.path);
    converted_param.compute_sha256 = api_options_param.compute_sha256;
    converted_param.compute_executable_metadata =
        api_options_param.compute_executable_metadata;
    converted_options.push_back(std::move(converted_param));
  }
  return converted_options;
}

absl::optional<ParsedSignalsError> ConvertFileSystemInfoResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>*
        arg_list) {
  auto error = TryParseError(aggregation_response,
                             aggregation_response.file_system_info_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>
      api_responses;
  const auto& file_system_signal_values =
      aggregation_response.file_system_info_response.value();
  for (const auto& file_system_item :
       file_system_signal_values.file_system_items) {
    api::enterprise_reporting_private::GetFileSystemInfoResponse response;
    response.path = file_system_item.file_path.AsUTF8Unsafe();
    response.presence = ConvertPresenceValue(file_system_item.presence);

    if (file_system_item.sha256_hash) {
      response.sha256_hash = EncodeHash(*file_system_item.sha256_hash);
    }

    if (file_system_item.executable_metadata) {
      const auto& executable_metadata =
          file_system_item.executable_metadata.value();

      response.is_running = executable_metadata.is_running;

      if (executable_metadata.public_keys_hashes) {
        response.public_keys_hashes =
            EncodeHashes(executable_metadata.public_keys_hashes.value());
      }

      response.product_name = executable_metadata.product_name;

      response.version = executable_metadata.version;
    }

    api_responses.push_back(std::move(response));
  }

  *arg_list = std::move(api_responses);
  return absl::nullopt;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

std::vector<device_signals::GetSettingsOptions> ConvertSettingsOptions(
    const std::vector<api::enterprise_reporting_private::GetSettingsOptions>&
        api_options) {
  std::vector<device_signals::GetSettingsOptions> converted_options;
  for (const auto& api_options_param : api_options) {
    device_signals::GetSettingsOptions converted_param;
    converted_param.path = api_options_param.path;
    converted_param.key = api_options_param.key;
    converted_param.get_value = api_options_param.get_value;
    converted_param.hive = ConvertHiveFromApi(api_options_param.hive);

    converted_options.push_back(std::move(converted_param));
  }
  return converted_options;
}

absl::optional<ParsedSignalsError> ConvertSettingsResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::GetSettingsResponse>*
        arg_list) {
  auto error = TryParseError(aggregation_response,
                             aggregation_response.settings_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::GetSettingsResponse>
      api_responses;
  const auto& settings_signal_values =
      aggregation_response.settings_response.value();
  for (const auto& settings_item : settings_signal_values.settings_items) {
    api::enterprise_reporting_private::GetSettingsResponse response;
    response.path = settings_item.path;
    response.key = settings_item.key;
    response.presence = ConvertPresenceValue(settings_item.presence);
    response.hive = ConvertHiveToApi(settings_item.hive);

    if (settings_item.setting_json_value) {
      response.value = settings_item.setting_json_value.value();
    }

    api_responses.push_back(std::move(response));
  }

  *arg_list = std::move(api_responses);
  return absl::nullopt;
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

absl::optional<ParsedSignalsError> ConvertAvProductsResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::AntiVirusSignal>* arg_list) {
  auto error = TryParseError(aggregation_response,
                             aggregation_response.av_signal_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::AntiVirusSignal>
      api_av_signals;
  const auto& av_response = aggregation_response.av_signal_response.value();
  const size_t upper_bound =
      std::min(kGeneralSignalUpperLimit, av_response.av_products.size());
  for (size_t i = 0U; i < upper_bound; ++i) {
    const auto& av_product = av_response.av_products[i];
    api::enterprise_reporting_private::AntiVirusSignal api_av_signal;
    api_av_signal.display_name = av_product.display_name;
    api_av_signal.product_id = av_product.product_id;

    switch (av_product.state) {
      case device_signals::AvProductState::kOn:
        api_av_signal.state = api::enterprise_reporting_private::
            AntiVirusProductState::ANTI_VIRUS_PRODUCT_STATE_ON;
        break;
      case device_signals::AvProductState::kOff:
        api_av_signal.state = api::enterprise_reporting_private::
            AntiVirusProductState::ANTI_VIRUS_PRODUCT_STATE_OFF;
        break;
      case device_signals::AvProductState::kSnoozed:
        api_av_signal.state = api::enterprise_reporting_private::
            AntiVirusProductState::ANTI_VIRUS_PRODUCT_STATE_SNOOZED;
        break;
      case device_signals::AvProductState::kExpired:
        api_av_signal.state = api::enterprise_reporting_private::
            AntiVirusProductState::ANTI_VIRUS_PRODUCT_STATE_EXPIRED;
        break;
    }

    api_av_signals.push_back(std::move(api_av_signal));
  }

  *arg_list = std::move(api_av_signals);
  return absl::nullopt;
}

absl::optional<ParsedSignalsError> ConvertHotfixesResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::HotfixSignal>* arg_list) {
  auto error = TryParseError(aggregation_response,
                             aggregation_response.hotfix_signal_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::HotfixSignal>
      api_hotfix_signals;
  const auto& hotfix_response =
      aggregation_response.hotfix_signal_response.value();
  const size_t upper_bound =
      std::min(kGeneralSignalUpperLimit, hotfix_response.hotfixes.size());
  for (size_t i = 0U; i < upper_bound; ++i) {
    const auto& hotfix = hotfix_response.hotfixes[i];
    api::enterprise_reporting_private::HotfixSignal api_hotfix;
    api_hotfix.hotfix_id = hotfix.hotfix_id;
    api_hotfix_signals.push_back(std::move(api_hotfix));
  }

  *arg_list = std::move(api_hotfix_signals);
  return absl::nullopt;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace extensions

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
