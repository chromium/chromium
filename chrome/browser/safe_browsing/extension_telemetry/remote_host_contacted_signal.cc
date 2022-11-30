// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

using RemoteHostInfo = ExtensionTelemetryReportRequest::SignalInfo::
    RemoteHostContactedInfo::RemoteHostInfo;

RemoteHostContactedSignal::RemoteHostContactedSignal(
    const extensions::ExtensionId& extension_id,
    const GURL& host_url,
    RemoteHostInfo::ProtocolType protocol)
    : ExtensionSignal(extension_id),
      remote_host_url_(host_url),
      protocol_(protocol) {}

RemoteHostContactedSignal::~RemoteHostContactedSignal() = default;

ExtensionSignalType RemoteHostContactedSignal::GetType() const {
  return ExtensionSignalType::kRemoteHostContacted;
}

}  // namespace safe_browsing
