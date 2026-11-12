// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_GRAMMATICAL_GENDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LANGUAGE_GRAMMATICAL_GENDER_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace language {
class GrammaticalGenderService;
}

// Owns the GrammaticalGenderService for each profile. Only regular profiles
// have a service; incognito profiles are redirected to their original
// profile's service, and other profiles (e.g. Guest and System) don't have
// one. The service is created lazily, the first time it's requested.
class GrammaticalGenderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static language::GrammaticalGenderService* GetForProfile(Profile* profile);
  static GrammaticalGenderServiceFactory* GetInstance();

  GrammaticalGenderServiceFactory(const GrammaticalGenderServiceFactory&) =
      delete;
  GrammaticalGenderServiceFactory& operator=(
      const GrammaticalGenderServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GrammaticalGenderServiceFactory>;

  GrammaticalGenderServiceFactory();
  ~GrammaticalGenderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_LANGUAGE_GRAMMATICAL_GENDER_SERVICE_FACTORY_H_
