// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/custom_handlers/chrome_protocol_handler_registry_delegate.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

// static
ProtocolHandlerRegistryFactory* ProtocolHandlerRegistryFactory::GetInstance() {
  static base::NoDestructor<ProtocolHandlerRegistryFactory> instance;
  return instance.get();
}

// static
custom_handlers::ProtocolHandlerRegistry*
ProtocolHandlerRegistryFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<custom_handlers::ProtocolHandlerRegistry*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ProtocolHandlerRegistryFactory::ProtocolHandlerRegistryFactory()
    : ProfileKeyedServiceFactory(
          "ProtocolHandlerRegistry",
          // Allows the produced registry to be used in incognito mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ProtocolHandlerRegistryFactory::~ProtocolHandlerRegistryFactory() = default;

// Will be created when initializing profile_io_data, so we might
// as well have the framework create this along with other
// PKSs to preserve orderly civic conduct :)
bool
ProtocolHandlerRegistryFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Do not create this service for tests. MANY tests will fail
// due to the threading requirements of this service. ALSO,
// not creating this increases test isolation (which is GOOD!)
bool ProtocolHandlerRegistryFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
ProtocolHandlerRegistryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  PrefService* prefs = user_prefs::UserPrefs::Get(context);
  DCHECK(prefs);
  return custom_handlers::ProtocolHandlerRegistry::Create(
      prefs, std::make_unique<ChromeProtocolHandlerRegistryDelegate>());
}
