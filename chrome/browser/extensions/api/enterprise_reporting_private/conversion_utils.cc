// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/conversion_utils.h"

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
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>*
        arg_list) {
  auto error = TryParseError(response, response.file_system_info_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>
      api_responses;
  const auto& file_system_signal_values =
      response.file_system_info_response.value();
  for (const auto& file_system_item :
       file_system_signal_values.file_system_items) {
    api::enterprise_reporting_private::GetFileSystemInfoResponse response;
    response.path = file_system_item.file_path.AsUTF8Unsafe();
    response.presence = ConvertPresenceValue(file_system_item.presence);

    if (file_system_item.sha256_hash) {
      response.sha256_hash = std::make_unique<std::string>(
          EncodeHash(file_system_item.sha256_hash.value()));
    }

    if (file_system_item.executable_metadata) {
      const auto& executable_metadata =
          file_system_item.executable_metadata.value();

      response.is_running =
          std::make_unique<bool>(executable_metadata.is_running);

      if (executable_metadata.public_key_sha256) {
        response.public_key_sha256 = std::make_unique<std::string>(
            EncodeHash(executable_metadata.public_key_sha256.value()));
      }

      if (executable_metadata.product_name) {
        response.product_name = std::make_unique<std::string>(
            executable_metadata.product_name.value());
      }

      if (executable_metadata.version) {
        response.version =
            std::make_unique<std::string>(executable_metadata.version.value());
      }
    }

    api_responses.push_back(std::move(response));
  }

  *arg_list = std::move(api_responses);
  return absl::nullopt;
}

#if BUILDFLAG(IS_WIN)

absl::optional<ParsedSignalsError> ConvertAvProductsResponse(
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::AntiVirusSignal>* arg_list) {
  auto error = TryParseError(response, response.av_signal_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::AntiVirusSignal>
      api_av_signals;
  const auto& av_response = response.av_signal_response.value();
  for (const auto& av_product : av_response.av_products) {
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
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::HotfixSignal>* arg_list) {
  auto error = TryParseError(response, response.hotfix_signal_response);
  if (error) {
    return error.value();
  }

  std::vector<api::enterprise_reporting_private::HotfixSignal>
      api_hotfix_signals;
  const auto& hotfix_response = response.hotfix_signal_response.value();
  for (const auto& hotfix : hotfix_response.hotfixes) {
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
