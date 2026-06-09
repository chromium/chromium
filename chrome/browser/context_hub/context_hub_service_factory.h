// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace context_hub {
class ContextHubService;
}  // namespace context_hub

class ContextHubServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static context_hub::ContextHubService* GetForProfile(Profile* profile);
  static ContextHubServiceFactory* GetInstance();

  ContextHubServiceFactory(const ContextHubServiceFactory&) = delete;
  ContextHubServiceFactory& operator=(const ContextHubServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ContextHubServiceFactory>;

  ContextHubServiceFactory();
  ~ContextHubServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_CONTEXT_HUB_CONTEXT_HUB_SERVICE_FACTORY_H_
