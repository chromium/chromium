// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"

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
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

KeyedService* AutofillPrivateEventRouterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return AutofillPrivateEventRouter::Create(context);
}

bool AutofillPrivateEventRouterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
