// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/task_module/task_module_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/task_module/task_module_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
TaskModuleService* TaskModuleServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TaskModuleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TaskModuleServiceFactory* TaskModuleServiceFactory::GetInstance() {
  return base::Singleton<TaskModuleServiceFactory>::get();
}

TaskModuleServiceFactory::TaskModuleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TaskModuleService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

TaskModuleServiceFactory::~TaskModuleServiceFactory() = default;

KeyedService* TaskModuleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(context)
          ->GetURLLoaderFactoryForBrowserProcess();
  return new TaskModuleService(url_loader_factory,
                               Profile::FromBrowserContext(context),
                               g_browser_process->GetApplicationLocale());
}
