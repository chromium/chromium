// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace signin {
class AccountPreviewDataService;
}

// KeyedService factory that creates the Profile-associated
// `AccountPreviewDataService`.
class AccountPreviewDataServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the AccountPreviewDataService instance associated with this
  // profile (creating one if it does not exist). Returns null if `profile`
  // is not regular user profile.
  static signin::AccountPreviewDataService* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static AccountPreviewDataServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<AccountPreviewDataServiceFactory>;

  AccountPreviewDataServiceFactory();
  ~AccountPreviewDataServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
