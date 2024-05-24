// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_METRICS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class SigninMetricsService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Singleton that manages the `SigninMetricsService` service per profile.
class SigninMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SigninMetricsService* GetForProfile(Profile* profile);

  // Returns an instance of the SigninMetricsServiceFactory singleton.
  static SigninMetricsServiceFactory* GetInstance();

  SigninMetricsServiceFactory(const SigninMetricsServiceFactory&) = delete;
  SigninMetricsServiceFactory& operator=(const SigninMetricsServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<SigninMetricsServiceFactory>;

  SigninMetricsServiceFactory();
  ~SigninMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_METRICS_SERVICE_FACTORY_H_
