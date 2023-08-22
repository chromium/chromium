// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class CWSInfoService;

// Singleton that produces CWSInfoService objects, one for each active Profile.
class CWSInfoServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CWSInfoService* GetForProfile(Profile* profile);
  static CWSInfoServiceFactory* GetInstance();

  CWSInfoServiceFactory(const CWSInfoServiceFactory&) = delete;
  CWSInfoServiceFactory& operator=(const CWSInfoServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<CWSInfoServiceFactory>;

  CWSInfoServiceFactory();
  ~CWSInfoServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_FACTORY_H_
