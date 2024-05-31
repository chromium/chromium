// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "extensions/common/extension.h"

namespace enterprise_connectors {
// An event router that collects extension telemetry reports and then sends
// events to reporting server.
class ExtensionTelemetryEventRouter {
 public:
  explicit ExtensionTelemetryEventRouter(content::BrowserContext* context);
  ExtensionTelemetryEventRouter(const ExtensionTelemetryEventRouter&) = delete;
  ExtensionTelemetryEventRouter& operator=(
      const ExtensionTelemetryEventRouter&) = delete;
  ExtensionTelemetryEventRouter(ExtensionTelemetryEventRouter&&) = delete;
  ExtensionTelemetryEventRouter& operator=(ExtensionTelemetryEventRouter&&) =
      delete;

  ~ExtensionTelemetryEventRouter();

  std::string GetLocationString(extensions::mojom::ManifestLocation location);
  void UploadTelemetryReport(content::BrowserContext* browser_context,
                             const extensions::Extension* extension);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_H_
