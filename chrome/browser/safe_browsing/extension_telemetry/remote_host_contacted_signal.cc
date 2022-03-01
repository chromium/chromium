// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"

namespace safe_browsing {

RemoteHostContactedSignal::RemoteHostContactedSignal(
    const extensions::ExtensionId& extension_id,
    const GURL& host_url)
    : ExtensionSignal(extension_id), contacted_host_url_(host_url) {}

RemoteHostContactedSignal::~RemoteHostContactedSignal() = default;

ExtensionSignalType RemoteHostContactedSignal::GetType() const {
  return ExtensionSignalType::kRemoteHostContacted;
}

}  // namespace safe_browsing
