// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

namespace safe_browsing {

DOMAccessSignal::DOMAccessSignal(const extensions::ExtensionId& extension_id,
                                 std::string api_name,
                                 std::string url,
                                 AccessType access_type,
                                 base::Time timestamp)
    : ExtensionSignal(extension_id),
      api_name_(std::move(api_name)),
      access_type_(access_type),
      timestamp_(timestamp) {
  url_ = SanitizeURLWithoutFilename(std::move(url));
}

DOMAccessSignal::~DOMAccessSignal() = default;

ExtensionSignalType DOMAccessSignal::GetType() const {
  return ExtensionSignalType::kDOMAccess;
}

std::string DOMAccessSignal::GetAggregationKey() const {
  return base::JoinString(
      {api_name_, url_, base::NumberToString(static_cast<int>(access_type_))},
      "|");
}

}  // namespace safe_browsing
