// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// static
SystemIndicatorManager* SystemIndicatorManagerFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<SystemIndicatorManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SystemIndicatorManagerFactory* SystemIndicatorManagerFactory::GetInstance() {
  return base::Singleton<SystemIndicatorManagerFactory>::get();
}

SystemIndicatorManagerFactory::SystemIndicatorManagerFactory()
    : BrowserContextKeyedServiceFactory(
        "SystemIndicatorManager",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

SystemIndicatorManagerFactory::~SystemIndicatorManagerFactory() {}

KeyedService* SystemIndicatorManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray == NULL)
    return NULL;

  return new SystemIndicatorManager(static_cast<Profile*>(profile),
                                    status_tray);
}

bool SystemIndicatorManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // The SystemIndicatorManager monitors extensions being loaded and unloaded
  // to check if they have system indicators specified. In order to observe
  // these, it needs to be created at profile initialization.
  return true;
}

}  // namespace extensions
