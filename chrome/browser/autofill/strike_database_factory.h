// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class Profile;

namespace autofill {

class StrikeDatabase;

// Singleton that owns all StrikeDatabases and associates them with
// Profiles.
class StrikeDatabaseFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the StrikeDatabase for |profile|, creating it if it is not
  // yet created.
  static StrikeDatabase* GetForProfile(Profile* profile);

  static StrikeDatabaseFactory* GetInstance();

  StrikeDatabaseFactory(const StrikeDatabaseFactory&) = delete;
  StrikeDatabaseFactory& operator=(const StrikeDatabaseFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<StrikeDatabaseFactory>;

  StrikeDatabaseFactory();
  ~StrikeDatabaseFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_STRIKE_DATABASE_FACTORY_H_
