// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/common/manifest_handlers/description_info.h"

using ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent;

namespace enterprise_connectors {


ExtensionInstallEventRouter::ExtensionInstallEventRouter(
    content::BrowserContext* context) {
  extension_registry_ = extensions::ExtensionRegistry::Get(context);
  reporting_client_ = RealtimeReportingClientFactory::GetForProfile(context);

  DLOG_IF(ERROR, !extension_registry_)
      << "extension_registry_ is null. Observer not added.";
  if (extension_registry_ && reporting_client_) {
    extension_registry_->AddObserver(this);
  }
}

ExtensionInstallEventRouter::~ExtensionInstallEventRouter() {
  if (extension_registry_ && reporting_client_) {
    extension_registry_->RemoveObserver(this);
  }
}

void ExtensionInstallEventRouter::ReportExtensionInstallEvent(
    const extensions::Extension* extension,
    const BrowserExtensionInstallEvent::ExtensionAction extension_action) {

  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kExtensionInstallEvent) == 0) {
    return;
  }

  // Build the event proto.
  ::chrome::cros::reporting::proto::Event event;
  auto* extension_event = event.mutable_browser_extension_install_event();
  extension_event->set_id(extension->id());
  extension_event->set_name(extension->name());
  extension_event->set_description(
      extensions::DescriptionInfo::GetDescription(*extension));
  extension_event->set_extension_action_type(extension_action);
  extension_event->set_extension_version(extension->GetVersionForDisplay());
  extension_event->set_extension_source(GetExtensionSource(extension));
  extension_event->set_profile_user_name(
      reporting_client_->GetProfileUserName());
  extension_event->set_profile_identifier(
      reporting_client_->GetProfileIdentifier());

  reporting_client_->ReportEvent(std::move(event), std::move(settings.value()));
}


void ExtensionInstallEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  ReportExtensionInstallEvent(
      extension,
      is_update ? BrowserExtensionInstallEvent::ExtensionAction::
                      BrowserExtensionInstallEvent_ExtensionAction_UPDATE
                : BrowserExtensionInstallEvent::ExtensionAction::
                      BrowserExtensionInstallEvent_ExtensionAction_INSTALL);
}

void ExtensionInstallEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  ReportExtensionInstallEvent(
      extension, BrowserExtensionInstallEvent::ExtensionAction::
                     BrowserExtensionInstallEvent_ExtensionAction_UNINSTALL);
}

// static
BrowserExtensionInstallEvent::ExtensionSource
ExtensionInstallEventRouter::GetExtensionSource(
    const extensions::Extension* extension) {
  if (extension->location() ==
      extensions::mojom::ManifestLocation::kComponent) {
    return BrowserExtensionInstallEvent::ExtensionSource::
        BrowserExtensionInstallEvent_ExtensionSource_COMPONENT;
  } else if (extension->from_webstore()) {
    return BrowserExtensionInstallEvent::ExtensionSource::
        BrowserExtensionInstallEvent_ExtensionSource_CHROME_WEBSTORE;
  } else {
    return BrowserExtensionInstallEvent::ExtensionSource::
        BrowserExtensionInstallEvent_ExtensionSource_EXTERNAL;
  }
}

ExtensionInstallEventRouterFactory*
ExtensionInstallEventRouterFactory::GetInstance() {
  static base::NoDestructor<ExtensionInstallEventRouterFactory> instance;
  return instance.get();
}

ExtensionInstallEventRouter*
ExtensionInstallEventRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionInstallEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ExtensionInstallEventRouterFactory::ExtensionInstallEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionInstallEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(RealtimeReportingClientFactory::GetInstance());
}

ExtensionInstallEventRouterFactory::~ExtensionInstallEventRouterFactory() =
    default;

bool ExtensionInstallEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
ExtensionInstallEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionInstallEventRouter>(context);
}

content::BrowserContext*
ExtensionInstallEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Do not construct the router if extensions are disabled for the given
  // context.
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(context)) {
    return nullptr;
  }

  return context;
}

}  // namespace enterprise_connectors
