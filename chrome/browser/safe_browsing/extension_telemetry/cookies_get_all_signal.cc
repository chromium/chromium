// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_util.h"

namespace safe_browsing {

CookiesGetAllSignal::CookiesGetAllSignal(
    const extensions::ExtensionId& extension_id,
    const std::string& domain,
    const std::string& name,
    const std::string& path,
    std::optional<bool> secure,
    const std::string& store_id,
    const std::string& url,
    std::optional<bool> is_session,
    extensions::StackTrace js_callstack)
    : ExtensionSignal(extension_id),
      domain_(domain),
      name_(name),
      path_(path),
      secure_(secure),
      store_id_(store_id),
      is_session_(is_session),
      js_callstack_(std::move(js_callstack)) {
  url_ = SanitizeURLWithoutFilename(url);
}

CookiesGetAllSignal::~CookiesGetAllSignal() = default;

ExtensionSignalType CookiesGetAllSignal::GetType() const {
  return ExtensionSignalType::kCookiesGetAll;
}

std::string CookiesGetAllSignal::getUniqueArgSetId() const {
  std::string secure_string;
  if (secure_.has_value()) {
    secure_string = base::NumberToString(secure_.value());
  }

  std::string is_session_string;
  if (is_session_.has_value()) {
    is_session_string = base::NumberToString(is_session_.value());
  }

  return base::JoinString({domain_, name_, path_, secure_string, store_id_,
                           url_, is_session_string},
                          ",");
}

}  // namespace safe_browsing
