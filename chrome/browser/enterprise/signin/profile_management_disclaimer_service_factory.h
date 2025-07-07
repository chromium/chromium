// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ProfileManagementDisclaimerService;
class Profile;

class ProfileManagementDisclaimerServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ProfileManagementDisclaimerService* GetForProfile(Profile* profile);
  static ProfileManagementDisclaimerServiceFactory* GetInstance();

  ProfileManagementDisclaimerServiceFactory(
      const ProfileManagementDisclaimerServiceFactory&) = delete;
  ProfileManagementDisclaimerServiceFactory& operator=(
      const ProfileManagementDisclaimerServiceFactory&) = delete;

  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend class base::NoDestructor<ProfileManagementDisclaimerServiceFactory>;
  ProfileManagementDisclaimerServiceFactory();
  ~ProfileManagementDisclaimerServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_PROFILE_MANAGEMENT_DISCLAIMER_SERVICE_FACTORY_H_
