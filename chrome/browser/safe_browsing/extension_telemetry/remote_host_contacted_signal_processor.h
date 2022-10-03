// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

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
  using RemoteHostInfo = ExtensionTelemetryReportRequest::SignalInfo::
      RemoteHostContactedInfo::RemoteHostInfo;
  // Maps remote hosts url to contact count.
  using RemoteHostURLs = base::flat_map<std::string, uint32_t>;
  // Maps connection protocols to remote hosts contacted.
  using RemoteHostURLsByConnectionProtocol =
      base::flat_map<RemoteHostInfo::ProtocolType, RemoteHostURLs>;
  // Maps extension id to remote hosts contacted.
  using RemoteHostInfoPerExtension =
      base::flat_map<extensions::ExtensionId,
                     RemoteHostURLsByConnectionProtocol>;
  RemoteHostInfoPerExtension remote_host_info_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
