// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

namespace safe_browsing {

CookiesGetSignal::CookiesGetSignal(const extensions::ExtensionId& extension_id,
                                   const std::string& name,
                                   const std::string& store_id,
                                   const std::string& url,
                                   extensions::StackTrace js_callstack)
    : ExtensionSignal(extension_id),
      name_(name),
      store_id_(store_id),
      js_callstack_(std::move(js_callstack)) {
  url_ = SanitizeURLWithoutFilename(url);
}

CookiesGetSignal::~CookiesGetSignal() = default;

ExtensionSignalType CookiesGetSignal::GetType() const {
  return ExtensionSignalType::kCookiesGet;
}

std::string CookiesGetSignal::getUniqueArgSetId() const {
  return base::JoinString({name_, store_id_, url_}, ",");
}

}  // namespace safe_browsing
