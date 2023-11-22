// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_PROVIDER_STATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_PROVIDER_STATE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;
struct ProviderStateService;

namespace content {
class BrowserContext;
}

class ProviderStateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ProviderStateService* GetForProfile(Profile* profile);
  static ProviderStateServiceFactory* GetInstance();

  ProviderStateServiceFactory(const ProviderStateServiceFactory&) = delete;
  ProviderStateServiceFactory& operator=(const ProviderStateServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<ProviderStateServiceFactory>;

  ProviderStateServiceFactory();
  ~ProviderStateServiceFactory() override;

  // Overrides from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_PROVIDER_STATE_SERVICE_FACTORY_H_
