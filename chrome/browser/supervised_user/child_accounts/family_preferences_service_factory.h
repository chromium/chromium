// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Do not move to components/ until profile dependency is removed.
#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_PREFERENCES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_PREFERENCES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/family_preferences_service.h"
#include "content/public/browser/browser_context.h"

namespace supervised_user {
class FamilyPreferencesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FamilyPreferencesService* GetForProfile(Profile* profile);

  static FamilyPreferencesServiceFactory* GetInstance();

  FamilyPreferencesServiceFactory(const FamilyPreferencesServiceFactory&) =
      delete;
  FamilyPreferencesServiceFactory& operator=(
      const FamilyPreferencesServiceFactory&) = delete;

 private:
  friend base::NoDestructor<FamilyPreferencesServiceFactory>;

  FamilyPreferencesServiceFactory();
  ~FamilyPreferencesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_FAMILY_PREFERENCES_SERVICE_FACTORY_H_
