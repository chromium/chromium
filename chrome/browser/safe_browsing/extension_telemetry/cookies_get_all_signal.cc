// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include <sstream>
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

namespace safe_browsing {

CookiesGetAllSignal::CookiesGetAllSignal(
    const extensions::ExtensionId& extension_id,
    const std::string& domain,
    const std::string& name,
    const std::string& path,
    bool secure,
    const std::string& store_id,
    const std::string& url,
    bool is_session)
    : ExtensionSignal(extension_id),
      domain_(domain),
      name_(name),
      path_(path),
      secure_(secure),
      store_id_(store_id),
      is_session_(is_session) {
  url_ = SanitizeURLWithoutFilename(url);
}

CookiesGetAllSignal::~CookiesGetAllSignal() = default;

ExtensionSignalType CookiesGetAllSignal::GetType() const {
  return ExtensionSignalType::kCookiesGetAll;
}

std::string CookiesGetAllSignal::getUniqueArgSetId() const {
  std::stringstream ss;
  ss << domain_ << name_ << path_ << secure_ << store_id_ << url_
     << is_session_;
  return ss.str();
}

}  // namespace safe_browsing
