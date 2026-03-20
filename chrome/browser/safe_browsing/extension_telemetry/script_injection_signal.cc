// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"

#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {
constexpr size_t kMaxArgLength = 1024;
constexpr char kTruncatedMarker[] = "[TRUNC]";
}  // namespace
ScriptInjectionSignal::ScriptInjectionSignal(
    const extensions::ExtensionId& extension_id,
    std::string api_name,
    std::string url,
    std::vector<std::string> args_list,
    base::Time timestamp)
    : ExtensionSignal(extension_id),
      api_name_(std::move(api_name)),
      args_list_(std::move(args_list)),
      timestamp_(timestamp) {
  url_ = SanitizeURLWithoutFilename(std::move(url));

  // Enforce an upper bound on all arguments to prevent memory bloat
  // from massive script fragments (e.g., via executeScript).
  for (std::string& arg : args_list_) {
    if (arg.length() > kMaxArgLength) {
      arg.resize(kMaxArgLength);
      arg += kTruncatedMarker;
    }

    // Standard URLs (e.g. script src) are sanitized to remove query/ref
    // while keeping the filename.
    GURL gurl(arg);
    if (gurl.IsStandard()) {
      arg = SanitizeURLWithoutQueryAndRef(std::move(arg));
    }
  }
}

ScriptInjectionSignal::~ScriptInjectionSignal() = default;

ExtensionSignalType ScriptInjectionSignal::GetType() const {
  return ExtensionSignalType::kScriptInjection;
}

std::string ScriptInjectionSignal::GetAggregationKey() const {
  return base::JoinString({api_name_, url_, base::JoinString(args_list_, "|")},
                          "|");
}

}  // namespace safe_browsing
