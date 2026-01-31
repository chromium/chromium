// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace contextual_tasks {

class ContextualTasksUiService;

class ContextualTasksUiServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ContextualTasksUiServiceFactory(const ContextualTasksUiServiceFactory&) =
      delete;
  ContextualTasksUiServiceFactory& operator=(
      const ContextualTasksUiServiceFactory&) = delete;

  static ContextualTasksUiServiceFactory* GetInstance();

  static ContextualTasksUiService* GetForBrowserContext(
      content::BrowserContext* context);
  static ContextualTasksUiService* GetForBrowserContextIfExists(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ContextualTasksUiServiceFactory>;

  ContextualTasksUiServiceFactory();
  ~ContextualTasksUiServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_FACTORY_H_
