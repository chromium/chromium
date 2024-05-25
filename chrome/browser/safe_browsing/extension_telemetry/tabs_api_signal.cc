// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

namespace safe_browsing {

TabsApiSignal::TabsApiSignal(const extensions::ExtensionId& extension_id,
                             TabsApiInfo::ApiMethod api_method,
                             const std::string& current_url,
                             const std::string& new_url,
                             extensions::StackTrace js_callstack)
    : ExtensionSignal(extension_id),
      api_method_(api_method),
      js_callstack_(std::move(js_callstack)) {
  if (!current_url.empty()) {
    current_url_ = SanitizeURLWithoutFilename(current_url);
  }
  if (!new_url.empty()) {
    new_url_ = SanitizeURLWithoutFilename(new_url);
  }
}

TabsApiSignal::~TabsApiSignal() = default;

ExtensionSignalType TabsApiSignal::GetType() const {
  return ExtensionSignalType::kTabsApi;
}

std::string TabsApiSignal::GetUniqueCallDetailsId() const {
  return base::JoinString({base::NumberToString(static_cast<int>(api_method_)),
                           current_url_, new_url_},
                          ",");
}

}  // namespace safe_browsing
