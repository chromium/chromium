// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

using RemoteHostInfo = ExtensionTelemetryReportRequest::SignalInfo::
    RemoteHostContactedInfo::RemoteHostInfo;

// A signal that is created when an extension initiates a web request.
class RemoteHostContactedSignal : public ExtensionSignal {
 public:
  RemoteHostContactedSignal(const extensions::ExtensionId& extension_id,
                            const GURL& host_url,
                            RemoteHostInfo::ProtocolType protocol);
  ~RemoteHostContactedSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  const GURL& remote_host_url() const { return remote_host_url_; }

  const RemoteHostInfo::ProtocolType& protocol_type() const {
    return protocol_;
  }

 protected:
  // Url of the remote contacted host.
  GURL remote_host_url_;

  // The protocol used to contact the remote host.
  RemoteHostInfo::ProtocolType protocol_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_
