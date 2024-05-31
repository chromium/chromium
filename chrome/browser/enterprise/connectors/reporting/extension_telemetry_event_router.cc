// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

namespace enterprise_connectors {

namespace {

constexpr char kKeyExtensionId[] = "id";
constexpr char kKeyExtensionVersion[] = "extension_version";
constexpr char kKeyExtensionName[] = "name";
constexpr char kKeyExtensionSource[] = "extension_source";
constexpr char kKeyProfileUserName[] = "profileUserName";

// Install locations corresponding to
// chrome.cros.reporting.proto.ExtensionTelemetryEvent.InstallLocation
constexpr char kUnknownInstallLocation[] = "UNKNOWN_LOCATION";
constexpr char kInternalInstallLocation[] = "INTERNAL";
constexpr char kExternalPrefInstallLocation[] = "EXTERNAL_PREF";
constexpr char kExternalRegistryInstallLocation[] = "EXTERNAL_REGISTRY";
constexpr char kUnpackedInstallLocation[] = "UNPACKED";
constexpr char kComponentInstallLocation[] = "COMPONENT";
constexpr char kExternalPrefDownloadInstallLocation[] =
    "EXTERNAL_PREF_DOWNLOAD";
constexpr char kExternalPolicyDownloadInstallLocation[] =
    "EXTERNAL_POLICY_DOWNLOAD";
constexpr char kCommandLineInstallLocation[] = "COMMAND_LINE";
constexpr char kExternalPolicyInstallLocation[] = "EXTERNAL_POLICY";
constexpr char kExternalComponentInstallLocation[] = "EXTERNAL_COMPONENT";

}  // namespace

ExtensionTelemetryEventRouter::ExtensionTelemetryEventRouter(
    content::BrowserContext* context) {}

ExtensionTelemetryEventRouter::~ExtensionTelemetryEventRouter() = default;

std::string ExtensionTelemetryEventRouter::GetLocationString(
    extensions::mojom::ManifestLocation location) {
  switch (location) {
    case extensions::mojom::ManifestLocation::kInternal:
      return kInternalInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalPref:
      return kExternalPrefInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalRegistry:
      return kExternalRegistryInstallLocation;
    case extensions::mojom::ManifestLocation::kUnpacked:
      return kUnpackedInstallLocation;
    case extensions::mojom::ManifestLocation::kComponent:
      return kComponentInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalPrefDownload:
      return kExternalPrefDownloadInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalPolicyDownload:
      return kExternalPolicyDownloadInstallLocation;
    case extensions::mojom::ManifestLocation::kCommandLine:
      return kCommandLineInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalPolicy:
      return kExternalPolicyInstallLocation;
    case extensions::mojom::ManifestLocation::kExternalComponent:
      return kExternalComponentInstallLocation;
    case extensions::mojom::ManifestLocation::kInvalidLocation:
      return kUnknownInstallLocation;
  }
}

void ExtensionTelemetryEventRouter::UploadTelemetryReport(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!base::FeatureList::IsEnabled(kExtensionTelemetryEventsEnabled)) {
    return;
  }

  auto* reporting_client =
      RealtimeReportingClientFactory::GetForProfile(browser_context);
  if (!reporting_client) {
    return;
  }
  std::optional<ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          ReportingServiceSettings::kExtensionTelemetryEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyExtensionId, extension->id());
  event.Set(kKeyExtensionName, extension->name());
  event.Set(kKeyExtensionVersion, extension->GetVersionForDisplay());
  event.Set(kKeyExtensionSource, GetLocationString(extension->location()));
  event.Set(kKeyProfileUserName, reporting_client->GetProfileUserName());

  reporting_client->ReportRealtimeEvent(
      ReportingServiceSettings::kExtensionTelemetryEvent,
      std::move(settings.value()), std::move(event));
}

}  // namespace enterprise_connectors
