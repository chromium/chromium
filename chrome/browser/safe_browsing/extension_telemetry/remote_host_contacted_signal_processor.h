// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
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
  // Maps remote hosts url to contact count.
  using RemoteHostURLs = base::flat_map<std::string, uint32_t>;
  // Maps extension id to (remote host url, contact count).
  using RemoteHostURLsPerExtension =
      base::flat_map<extensions::ExtensionId, RemoteHostURLs>;
  RemoteHostURLsPerExtension remote_host_url_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_PROCESSOR_H_
