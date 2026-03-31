// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GAPIS_GAPIS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GAPIS_GAPIS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace gapis {
class GapisService;
}  // namespace gapis

class GapisServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static gapis::GapisService* GetForProfile(Profile* profile);
  static GapisServiceFactory* GetInstance();

  GapisServiceFactory(const GapisServiceFactory&) = delete;
  GapisServiceFactory& operator=(const GapisServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GapisServiceFactory>;

  GapisServiceFactory();
  ~GapisServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_GAPIS_GAPIS_SERVICE_FACTORY_H_
