// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/browser_context_keyed_service_factories.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service.h"
#include "apps/app_restore_service_factory.h"
#include "apps/saved_files_service.h"
#include "apps/saved_files_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace apps {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  AppLifetimeMonitorFactory::GetInstance();
  AppRestoreServiceFactory::GetInstance();
}

void NotifyApplicationTerminating(content::BrowserContext* browser_context) {
  AppRestoreService* restore_service =
      AppRestoreServiceFactory::GetForBrowserContext(browser_context);
  CHECK(restore_service);
  restore_service->OnApplicationTerminating();

  SavedFilesService* saved_files_service =
      SavedFilesServiceFactory::GetForBrowserContextIfExists(browser_context);
  if (saved_files_service)
    saved_files_service->OnApplicationTerminating();
}

}  // namespace apps
