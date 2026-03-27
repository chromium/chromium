// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_INDIGO_INDIGO_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace indigo {

class IndigoService;

class IndigoServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static IndigoService* GetForProfile(Profile* profile);

  static IndigoServiceFactory* GetInstance();

  IndigoServiceFactory(const IndigoServiceFactory&) = delete;
  IndigoServiceFactory& operator=(const IndigoServiceFactory&) = delete;

 private:
  friend base::NoDestructor<IndigoServiceFactory>;

  IndigoServiceFactory();
  ~IndigoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_SERVICE_FACTORY_H_
