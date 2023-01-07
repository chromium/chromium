// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace accuracy_tips {
class AccuracyService;
}

// This factory helps construct and find the AccuracyService instance for a
// Profile.
class AccuracyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static accuracy_tips::AccuracyService* GetForProfile(Profile* profile);
  static AccuracyServiceFactory* GetInstance();

  AccuracyServiceFactory(const AccuracyServiceFactory&) = delete;
  AccuracyServiceFactory& operator=(const AccuracyServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<AccuracyServiceFactory>;

  AccuracyServiceFactory();
  ~AccuracyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_FACTORY_H_
