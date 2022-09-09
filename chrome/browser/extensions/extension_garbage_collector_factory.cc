// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_garbage_collector_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_garbage_collector.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "extensions/browser/extensions_browser_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#endif

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
  return base::Singleton<ExtensionGarbageCollectorFactory>::get();
}

ExtensionGarbageCollectorFactory::ExtensionGarbageCollectorFactory()
    : ProfileKeyedServiceFactory("ExtensionGarbageCollector") {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(InstallTrackerFactory::GetInstance());
}

ExtensionGarbageCollectorFactory::~ExtensionGarbageCollectorFactory() {}

// static
std::unique_ptr<KeyedService>
ExtensionGarbageCollectorFactory::BuildInstanceFor(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<ExtensionGarbageCollectorChromeOS>(context);
#else
  return std::make_unique<ExtensionGarbageCollector>(context);
#endif
}

KeyedService* ExtensionGarbageCollectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context).release();
}

bool ExtensionGarbageCollectorFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ExtensionGarbageCollectorFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
