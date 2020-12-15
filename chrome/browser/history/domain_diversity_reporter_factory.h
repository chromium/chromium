// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_
#define CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class DomainDiversityReporter;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

class DomainDiversityReporterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static DomainDiversityReporter* GetForProfile(Profile* profile);

  static DomainDiversityReporterFactory* GetInstance();

  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* profile);

 private:
  friend class base::NoDestructor<DomainDiversityReporterFactory>;

  DomainDiversityReporterFactory();
  ~DomainDiversityReporterFactory() override;

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(DomainDiversityReporterFactory);
};

#endif  // CHROME_BROWSER_HISTORY_DOMAIN_DIVERSITY_REPORTER_FACTORY_H_
