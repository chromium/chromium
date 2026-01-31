// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace supervised_user {

class SupervisedUserUrlFilteringService;

// Not available for over the counter profiles (URL filtering not available).
class SupervisedUserUrlFilteringServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static SupervisedUserUrlFilteringService* GetForProfile(Profile* profile);
  static SupervisedUserUrlFilteringService* GetForProfileIfExists(
      Profile* profile);

  static SupervisedUserUrlFilteringServiceFactory* GetInstance();

  SupervisedUserUrlFilteringServiceFactory(
      const SupervisedUserUrlFilteringServiceFactory&) = delete;
  SupervisedUserUrlFilteringServiceFactory& operator=(
      const SupervisedUserUrlFilteringServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<SupervisedUserUrlFilteringServiceFactory>;

  SupervisedUserUrlFilteringServiceFactory();
  ~SupervisedUserUrlFilteringServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_URL_FILTERING_SERVICE_FACTORY_H_
