// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"

namespace safe_browsing {

DeclarativeNetRequestSignal::DeclarativeNetRequestSignal(
    const extensions::ExtensionId& extension_id,
    const std::vector<extensions::api::declarative_net_request::Rule>& rules)
    : ExtensionSignal(extension_id), rules_(rules) {}

DeclarativeNetRequestSignal::~DeclarativeNetRequestSignal() = default;

ExtensionSignalType DeclarativeNetRequestSignal::GetType() const {
  return ExtensionSignalType::kDeclarativeNetRequest;
}

}  // namespace safe_browsing
