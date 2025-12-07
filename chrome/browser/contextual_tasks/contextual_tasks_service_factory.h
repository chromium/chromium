// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace contextual_tasks {
class ContextualTasksService;

// A factory to create a unique ContextualTasksService.
class ContextualTasksServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the ContextualTasksService for the profile. An empty service is
  // returned for incognito/guest.
  static ContextualTasksService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of ContextualTasksServiceFactory.
  static ContextualTasksServiceFactory* GetInstance();

  // Disallow copy/assign.
  ContextualTasksServiceFactory(const ContextualTasksServiceFactory&) = delete;
  void operator=(const ContextualTasksServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ContextualTasksServiceFactory>;

  ContextualTasksServiceFactory();
  ~ContextualTasksServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SERVICE_FACTORY_H_
