// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class AiDataKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static AiDataKeyedServiceFactory* GetInstance();

  static AiDataKeyedService* GetAiDataKeyedService(
      content::BrowserContext* browser_context);

  AiDataKeyedServiceFactory(const AiDataKeyedServiceFactory&) = delete;
  AiDataKeyedServiceFactory& operator=(const AiDataKeyedServiceFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<AiDataKeyedServiceFactory>;

  AiDataKeyedServiceFactory();
  ~AiDataKeyedServiceFactory() override;
};

#endif  // CHROME_BROWSER_AI_AI_DATA_KEYED_SERVICE_FACTORY_H_
