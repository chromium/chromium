// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

using RemoteHostInfo = ExtensionTelemetryReportRequest::SignalInfo::
    RemoteHostContactedInfo::RemoteHostInfo;

// A class that processes CRX web request signal/trigger data to generate
// telemetry reports.
class RemoteHostContactedSignalProcessor : public ExtensionSignalProcessor {
 public:
  RemoteHostContactedSignalProcessor();
  ~RemoteHostContactedSignalProcessor() override;

  RemoteHostContactedSignalProcessor(RemoteHostContactedSignalProcessor&) =
      delete;
  RemoteHostContactedSignalProcessor& operator=(
      const RemoteHostContactedSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

 protected:
  // Used to store unique remote host contacted signals in a map where the key
  // is a concatenated string of the signal fields.
  using RemoteHostsContacted = base::flat_map<std::string, RemoteHostInfo>;
  // Maps extension id to remote hosts contacted store entry.
  using RemoteHostsContactedPerExtension =
      base::flat_map<extensions::ExtensionId, RemoteHostsContacted>;
  RemoteHostsContactedPerExtension remote_host_contacted_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
