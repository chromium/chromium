// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class AIManagerKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AIManagerKeyedServiceFactory* GetInstance();

  static AIManagerKeyedService* GetAIManagerKeyedService(
      content::BrowserContext* browser_context);

  AIManagerKeyedServiceFactory(const AIManagerKeyedServiceFactory&) = delete;
  AIManagerKeyedServiceFactory& operator=(const AIManagerKeyedServiceFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<AIManagerKeyedServiceFactory>;
  FRIEND_TEST_ALL_PREFIXES(AIManagerKeyedServiceTest,
                           NoUAFWithInvalidOnDeviceModelPath);

  AIManagerKeyedServiceFactory();
  ~AIManagerKeyedServiceFactory() override;
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_FACTORY_H_
