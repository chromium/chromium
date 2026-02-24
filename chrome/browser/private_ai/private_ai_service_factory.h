// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

class Profile;

namespace private_ai {

class PrivateAiService;

class PrivateAiServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PrivateAiService* GetForProfile(Profile* profile);
  static PrivateAiServiceFactory* GetInstance();

  PrivateAiServiceFactory(const PrivateAiServiceFactory&) = delete;
  PrivateAiServiceFactory& operator=(const PrivateAiServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PrivateAiServiceFactory>;

  PrivateAiServiceFactory();
  ~PrivateAiServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace private_ai

#endif  // CHROME_BROWSER_PRIVATE_AI_PRIVATE_AI_SERVICE_FACTORY_H_
