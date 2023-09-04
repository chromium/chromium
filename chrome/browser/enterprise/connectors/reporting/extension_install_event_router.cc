// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"

#include "base/feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "extensions/browser/extension_registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// Key names used with when building the dictionary to pass to the real-time
// reporting API.
const char kKeyId[] = "id";
const char kKeyName[] = "name";
const char kKeyDescription[] = "description";

ExtensionInstallEventRouter::ExtensionInstallEventRouter(
    content::BrowserContext* context) {
  extension_registry_ = extensions::ExtensionRegistry::Get(context);
  reporting_client_ = RealtimeReportingClientFactory::GetForProfile(context);
}

ExtensionInstallEventRouter::~ExtensionInstallEventRouter() {
  if (extension_registry_ && reporting_client_) {
    extension_registry_->RemoveObserver(this);
  }
}

void ExtensionInstallEventRouter::StartObserving() {
  DLOG_IF(ERROR, !extension_registry_)
      << "extension_registry_ is null. Observer not added.";
  if (extension_registry_ && reporting_client_) {
    extension_registry_->AddObserver(this);
  }
}

void ExtensionInstallEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  absl::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          ReportingServiceSettings::kExtensionInstallEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyId, extension->id());
  event.Set(kKeyName, extension->name());
  event.Set(kKeyDescription, extension->description());

  reporting_client_->ReportRealtimeEvent(
      ReportingServiceSettings::kExtensionInstallEvent,
      std::move(settings.value()), std::move(event));
}

}  // namespace enterprise_connectors
