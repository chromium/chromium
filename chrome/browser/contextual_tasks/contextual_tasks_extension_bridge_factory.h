// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace contextual_tasks {
class ContextualTasksExtensionBridge;

class ContextualTasksExtensionBridgeFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ContextualTasksExtensionBridge* GetForProfile(Profile* profile);
  static ContextualTasksExtensionBridgeFactory* GetInstance();

 private:
  friend base::NoDestructor<ContextualTasksExtensionBridgeFactory>;

  ContextualTasksExtensionBridgeFactory();
  ~ContextualTasksExtensionBridgeFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSION_BRIDGE_FACTORY_H_
