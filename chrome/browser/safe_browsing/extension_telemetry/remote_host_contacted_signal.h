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
  // TODO(crbug.com/40913716): Remove old constructor once new RHC
  // interception flow is fully launched.
  RemoteHostContactedSignal(const extensions::ExtensionId& extension_id,
                            const GURL& host_url,
                            RemoteHostInfo::ProtocolType protocol);
  RemoteHostContactedSignal(const extensions::ExtensionId& extension_id,
                            const GURL& host_url,
                            RemoteHostInfo::ProtocolType protocol,
                            RemoteHostInfo::ContactInitiator contact_initiator);

  RemoteHostContactedSignal(const RemoteHostContactedSignal&);
  RemoteHostContactedSignal(RemoteHostContactedSignal&&);
  RemoteHostContactedSignal& operator=(const RemoteHostContactedSignal&);
  RemoteHostContactedSignal& operator=(RemoteHostContactedSignal&&);

  ~RemoteHostContactedSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Creates a unique id, which can be used to uniquely identify a remote host
  // contacted signal.
  std::string GetUniqueRemoteHostContactedId() const;

  const GURL& remote_host_url() const { return remote_host_url_; }

  RemoteHostInfo::ProtocolType protocol_type() const { return protocol_; }

  RemoteHostInfo::ContactInitiator contact_initiator() const {
    return contact_initiator_;
  }

 protected:
  // Url of the remote contacted host.
  GURL remote_host_url_;

  // The protocol used to contact the remote host.
  RemoteHostInfo::ProtocolType protocol_;

  // Request initiated by either extension (service worker or extension page) or
  // content script.
  RemoteHostInfo::ContactInitiator contact_initiator_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_
