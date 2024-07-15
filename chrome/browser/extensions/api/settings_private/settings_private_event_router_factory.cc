// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"

#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_delegate_factory.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SettingsPrivateEventRouter* SettingsPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<SettingsPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SettingsPrivateEventRouterFactory*
SettingsPrivateEventRouterFactory::GetInstance() {
  static base::NoDestructor<SettingsPrivateEventRouterFactory> instance;
  return instance.get();
}

SettingsPrivateEventRouterFactory::SettingsPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "SettingsPrivateEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(settings_private::GeneratedPrefsFactory::GetInstance());
  DependsOn(SettingsPrivateDelegateFactory::GetInstance());
}

SettingsPrivateEventRouterFactory::~SettingsPrivateEventRouterFactory() =
    default;

std::unique_ptr<KeyedService>
SettingsPrivateEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return SettingsPrivateEventRouter::Create(context);
}

bool SettingsPrivateEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SettingsPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
