// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"

namespace enterprise_connectors {
// An event router that collects extension telemetry reports and then sends
// events to reporting server.
class ExtensionTelemetryEventRouter : public KeyedService {
 public:
  // Key names used with when building the dictionary to pass to the real-time
  // reporting API. These matches proto defined in
  // google3/logs/proto/safebrowsing/csd_telemetry.proto
  static const char kKeyExtensionTelemetryReport[];
  static const char kKeyCreationTimeMsec[];
  static const char kKeyReports[];
  static const char kKeyExtension[];
  static const char kKeySignals[];
  static const char kKeyCookiesGetAllInfo[];
  static const char kKeyGetAllArgsInfo[];
  static const char kKeyCookiesGetInfo[];
  static const char kKeyGetArgsInfo[];
  static const char kKeyRemoteHostContactedInfo[];
  static const char kKeyRemoteHost[];
  static const char kKeyTabsApiInfo[];
  static const char kKeyCallDetails[];
  static const char kKeyId[];
  static const char kKeyVersion[];
  static const char kKeyName[];
  static const char kKeyInstallLocation[];
  static const char kKeyIsFromStore[];
  static const char kKeyUrl[];
  static const char kKeyConnectionProtocol[];
  static const char kKeyContactedBy[];
  static const char kKeyContactCount[];
  static const char kKeyDomain[];
  static const char kKeyPath[];
  static const char kKeySecure[];
  static const char kKeyStoreId[];
  static const char kKeyIsSession[];
  static const char kKeyCount[];
  static const char kKeyMethod[];
  static const char kKeyNewUrl[];
  static const char kKeyCurrentUrl[];
  static const char kKeyFileInfo[];
  static const char kKeyHash[];

  // Convenience method to get the service for a profile.
  static ExtensionTelemetryEventRouter* Get(Profile* profile);

  explicit ExtensionTelemetryEventRouter(content::BrowserContext* context);
  ExtensionTelemetryEventRouter(const ExtensionTelemetryEventRouter&) = delete;
  ExtensionTelemetryEventRouter& operator=(
      const ExtensionTelemetryEventRouter&) = delete;
  ExtensionTelemetryEventRouter(ExtensionTelemetryEventRouter&&) = delete;
  ExtensionTelemetryEventRouter& operator=(ExtensionTelemetryEventRouter&&) =
      delete;

  ~ExtensionTelemetryEventRouter() override;

  bool IsPolicyEnabled();
  // Uploads the `ExtensionTelemetryReportRequest` as a telemetry event to the
  // reporting server.
  void UploadTelemetryReport(
      std::unique_ptr<safe_browsing::ExtensionTelemetryReportRequest>
          telemetry_report_request);

 private:
  raw_ptr<content::BrowserContext> context_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_
