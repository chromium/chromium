// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)

#include <vector>

#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {
struct SignalsAggregationResponse;
enum class SignalCollectionError;
}  // namespace device_signals

namespace extensions {

// Parses and converts the Antivirus signal values from `response` into
// `arg_list`. If any error occurred during signal collection, it will be
// returned and `arg_list` will remain unchanged.
absl::optional<device_signals::SignalCollectionError> ConvertAvProductsResponse(
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::AntiVirusSignal>* arg_list);

// Parses and converts the Hotfix signal values from `response` into
// `arg_list`. If any error occurred during signal collection,  it will be
// returned and `arg_list` will remain unchanged.
absl::optional<device_signals::SignalCollectionError> ConvertHotfixesResponse(
    const device_signals::SignalsAggregationResponse& response,
    std::vector<api::enterprise_reporting_private::HotfixSignal>* arg_list);

}  // namespace extensions

#endif  // BUILDFLAG(IS_WIN)

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CONVERSION_UTILS_H_
