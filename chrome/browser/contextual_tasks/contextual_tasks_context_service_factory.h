// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace contextual_tasks {
class ContextualTasksContextService;

class ContextualTasksContextServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ContextualTasksContextService* GetForProfile(Profile* profile);
  static ContextualTasksContextServiceFactory* GetInstance();

  ContextualTasksContextServiceFactory(
      const ContextualTasksContextServiceFactory&) = delete;
  ContextualTasksContextServiceFactory& operator=(
      const ContextualTasksContextServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ContextualTasksContextServiceFactory>;

  ContextualTasksContextServiceFactory();
  ~ContextualTasksContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_FACTORY_H_
