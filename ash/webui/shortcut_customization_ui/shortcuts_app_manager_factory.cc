// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager_factory.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace ash::shortcut_ui {

// static
ShortcutsAppManager* ShortcutsAppManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ShortcutsAppManager*>(
      ShortcutsAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
ShortcutsAppManagerFactory* ShortcutsAppManagerFactory::GetInstance() {
  return base::Singleton<ShortcutsAppManagerFactory>::get();
}

ShortcutsAppManagerFactory::ShortcutsAppManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ShortcutsAppManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      local_search_service::LocalSearchServiceProxyFactory::GetInstance());
}

ShortcutsAppManagerFactory::~ShortcutsAppManagerFactory() = default;

content::BrowserContext* ShortcutsAppManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // The service should exist in incognito mode.
  return context;
}

KeyedService* ShortcutsAppManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ShortcutsAppManager(
      local_search_service::LocalSearchServiceProxyFactory::
          GetForBrowserContext(context),
      user_prefs::UserPrefs::Get(context));
}

bool ShortcutsAppManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash::shortcut_ui
