// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/browser_context_keyed_service_factories.h"

#include "chrome/browser/apps/platform_apps/app_load_service_factory.h"
#include "chrome/browser/apps/platform_apps/app_termination_observer_factory.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager_factory.h"

namespace chrome_apps {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  apps::AppLoadServiceFactory::GetInstance();
  AppShortcutManagerFactory::GetInstance();
  AppTerminationObserverFactory::GetInstance();
}

}  // namespace chrome_apps
