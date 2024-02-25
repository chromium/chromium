// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#include <optional>
#include <vector>

#include "chrome/common/extensions/api/enterprise_reporting_private.h"

namespace device_signals {
struct GetFileSystemInfoOptions;
struct GetSettingsOptions;
struct SignalsAggregationResponse;
enum class SignalCollectionError;
}  // namespace device_signals

namespace extensions {

struct ParsedSignalsError {
  device_signals::SignalCollectionError error;
  bool is_top_level_error;
};

// Converts GetFileSystemInfoOptions from the Extension API struct definition,
// `api_options`, to the device_signals component definition.
std::vector<device_signals::GetFileSystemInfoOptions>
ConvertFileSystemInfoOptions(
    const std::vector<
        api::enterprise_reporting_private::GetFileSystemInfoOptions>&
        api_options);

// Parses and converts the File System info signal values from
// `aggregation_response` into `arg_list`. If any error occurred during signal
// collection, it will be returned and `arg_list` will remain unchanged.
std::optional<ParsedSignalsError> ConvertFileSystemInfoResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::GetFileSystemInfoResponse>*
        arg_list);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Converts GetSettingsOptions from the Extension API struct definition,
// `api_options`, to the device_signals component definition.
std::vector<device_signals::GetSettingsOptions> ConvertSettingsOptions(
    const std::vector<api::enterprise_reporting_private::GetSettingsOptions>&
        api_options);

// Parses and converts the Settings signal values from `aggregation_response`
// into `arg_list`. If any error occurred during signal collection, it will be
// returned and `arg_list` will remain unchanged.
std::optional<ParsedSignalsError> ConvertSettingsResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::GetSettingsResponse>*
        arg_list);

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

// Parses and converts the Antivirus signal values from `aggregation_response`
// into `arg_list`. If any error occurred during signal collection, it will be
// returned and `arg_list` will remain unchanged.
std::optional<ParsedSignalsError> ConvertAvProductsResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::AntiVirusSignal>* arg_list);

// Parses and converts the Hotfix signal values from `aggregation_response` into
// `arg_list`. If any error occurred during signal collection,  it will be
// returned and `arg_list` will remain unchanged.
std::optional<ParsedSignalsError> ConvertHotfixesResponse(
    const device_signals::SignalsAggregationResponse& aggregation_response,
    std::vector<api::enterprise_reporting_private::HotfixSignal>* arg_list);

#endif  // BUILDFLAG(IS_WIN)

}  // namespace extensions

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_
