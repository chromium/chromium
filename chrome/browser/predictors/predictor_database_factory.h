// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace predictors {

class PredictorDatabase;

// Singleton that owns the PredictorDatabases and associates them with
// Profiles.
class PredictorDatabaseFactory : public ProfileKeyedServiceFactory {
 public:
  static PredictorDatabase* GetForProfile(Profile* profile);

  static PredictorDatabaseFactory* GetInstance();

  PredictorDatabaseFactory(const PredictorDatabaseFactory&) = delete;
  PredictorDatabaseFactory& operator=(const PredictorDatabaseFactory&) = delete;

 private:
  friend base::NoDestructor<PredictorDatabaseFactory>;

  PredictorDatabaseFactory();
  ~PredictorDatabaseFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTOR_DATABASE_FACTORY_H_
