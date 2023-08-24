// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}
namespace permissions {
class PredictionService;
}

class PredictionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static permissions::PredictionService* GetForProfile(Profile* profile);
  static PredictionServiceFactory* GetInstance();

  PredictionServiceFactory(const PredictionServiceFactory&) = delete;
  PredictionServiceFactory& operator=(const PredictionServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PredictionServiceFactory>;

  PredictionServiceFactory();
  ~PredictionServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_FACTORY_H_
