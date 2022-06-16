// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/conversion_utils.h"

#if BUILDFLAG(IS_WIN)

#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/win/win_types.h"

using SignalCollectionError = device_signals::SignalCollectionError;

namespace extensions {

namespace {

absl::optional<SignalCollectionError> TryParseError(
    const device_signals::SignalsAggregationResponse& response,
    const absl::optional<device_signals::BaseSignalResponse>& bundle) {
  absl::optional<std::string> error_string;
  if (response.top_level_error.has_value()) {
    return response.top_level_error.value();
  }

  if (!bundle.has_value()) {
    return SignalCollectionError::kMissingBundle;
  }

  return bundle->collection_error;
}

}  // namespace

absl::optional<SignalCollectionError> ConvertAvProductsResponse(
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::AntiVirusSignal>* arg_list) {
  auto error = TryParseError(response, response.av_signal_response);
  if (error.has_value()) {
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

absl::optional<SignalCollectionError> ConvertHotfixesResponse(
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::HotfixSignal>* arg_list) {
  auto error = TryParseError(response, response.hotfix_signal_response);
  if (error.has_value()) {
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

}  // namespace extensions

#endif  // BUILDFLAG(IS_WIN)
