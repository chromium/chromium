// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace supervised_user {
class SupervisedUserMetricsService;
}  // namespace supervised_user

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Singleton that owns SupervisedUserMetricsService object and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// SupervisedUserMetricsService.
class SupervisedUserMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SupervisedUserMetricsServiceFactory(
      const SupervisedUserMetricsServiceFactory&) = delete;
  SupervisedUserMetricsServiceFactory& operator=(
      const SupervisedUserMetricsServiceFactory&) = delete;

  static supervised_user::SupervisedUserMetricsService* GetForBrowserContext(
      content::BrowserContext* context);

  static SupervisedUserMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SupervisedUserMetricsServiceFactory>;

  SupervisedUserMetricsServiceFactory();
  ~SupervisedUserMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
