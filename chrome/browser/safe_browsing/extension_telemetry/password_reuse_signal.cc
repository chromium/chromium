// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/password_reuse_signal.h"

namespace safe_browsing {

PasswordReuseSignal::PasswordReuseSignal(
    const extensions::ExtensionId& extension_id,
    safe_browsing::PasswordReuseInfo password_reuse_info)
    : ExtensionSignal(extension_id),
      password_reuse_info_(password_reuse_info) {}

PasswordReuseSignal::~PasswordReuseSignal() = default;

ExtensionSignalType PasswordReuseSignal::GetType() const {
  return ExtensionSignalType::kPasswordReuse;
}

}  // namespace safe_browsing
