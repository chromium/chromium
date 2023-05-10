// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
AutofillPrivateEventRouter*
AutofillPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<AutofillPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutofillPrivateEventRouterFactory*
AutofillPrivateEventRouterFactory::GetInstance() {
  return base::Singleton<AutofillPrivateEventRouterFactory>::get();
}

AutofillPrivateEventRouterFactory::AutofillPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "AutofillPrivateEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
}

KeyedService* AutofillPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(1426498): pass router's dependencies directly instead of context.
  return AutofillPrivateEventRouter::Create(context);
}

bool AutofillPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
