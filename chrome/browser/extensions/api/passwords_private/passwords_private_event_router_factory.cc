// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"

#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
PasswordsPrivateEventRouter*
PasswordsPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<PasswordsPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PasswordsPrivateEventRouterFactory*
PasswordsPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<PasswordsPrivateEventRouterFactory>::get();
}

PasswordsPrivateEventRouterFactory::PasswordsPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "PasswordsPrivateEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

PasswordsPrivateEventRouterFactory::
    ~PasswordsPrivateEventRouterFactory() {
}

KeyedService* PasswordsPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return PasswordsPrivateEventRouter::Create(context);
}

bool PasswordsPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PasswordsPrivateEventRouterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
