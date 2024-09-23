// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class LookalikeUrlService;

class LookalikeUrlServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static LookalikeUrlService* GetForProfile(Profile* profile);

  static LookalikeUrlServiceFactory* GetInstance();

  LookalikeUrlServiceFactory(const LookalikeUrlServiceFactory&) = delete;
  LookalikeUrlServiceFactory& operator=(const LookalikeUrlServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<LookalikeUrlServiceFactory>;

  LookalikeUrlServiceFactory();

  ~LookalikeUrlServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LOOKALIKES_LOOKALIKE_URL_SERVICE_FACTORY_H_
