// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

#include <sstream>

namespace safe_browsing {

CookiesGetSignal::CookiesGetSignal(const extensions::ExtensionId& extension_id,
                                   const std::string& name,
                                   const std::string& store_id,
                                   const std::string& url)
    : ExtensionSignal(extension_id), name_(name), store_id_(store_id) {
  url_ = SanitizeURLWithoutFilename(url);
}

CookiesGetSignal::~CookiesGetSignal() = default;

ExtensionSignalType CookiesGetSignal::GetType() const {
  return ExtensionSignalType::kCookiesGet;
}

std::string CookiesGetSignal::getUniqueArgSetId() const {
  std::stringstream ss;
  ss << name_ << store_id_ << url_;
  return ss.str();
}

}  // namespace safe_browsing
