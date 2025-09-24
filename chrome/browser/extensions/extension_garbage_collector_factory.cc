// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

// static
ExtensionGarbageCollector*
ExtensionGarbageCollectorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionGarbageCollector*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionGarbageCollectorFactory*
ExtensionGarbageCollectorFactory::GetInstance() {
  static base::NoDestructor<ExtensionGarbageCollectorFactory> instance;
  return instance.get();
}

ExtensionGarbageCollectorFactory::ExtensionGarbageCollectorFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionGarbageCollector",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(InstallTrackerFactory::GetInstance());
}

ExtensionGarbageCollectorFactory::~ExtensionGarbageCollectorFactory() = default;

// static
std::unique_ptr<KeyedService>
ExtensionGarbageCollectorFactory::BuildInstanceFor(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<ExtensionGarbageCollectorChromeOS>(context);
#else
  return std::make_unique<ExtensionGarbageCollector>(context);
#endif
}

std::unique_ptr<KeyedService>
ExtensionGarbageCollectorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

bool ExtensionGarbageCollectorFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ExtensionGarbageCollectorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
