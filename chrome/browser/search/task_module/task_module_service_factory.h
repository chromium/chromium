// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class TaskModuleService;

// Factory to access the task module service for the current profile.
class TaskModuleServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TaskModuleService* GetForProfile(Profile* profile);
  static TaskModuleServiceFactory* GetInstance();

  TaskModuleServiceFactory(const TaskModuleServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TaskModuleServiceFactory>;

  TaskModuleServiceFactory();
  ~TaskModuleServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SEARCH_TASK_MODULE_TASK_MODULE_SERVICE_FACTORY_H_
