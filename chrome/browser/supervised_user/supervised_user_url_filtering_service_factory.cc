// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"

namespace supervised_user {

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserUrlFilteringService*
SupervisedUserUrlFilteringServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<SupervisedUserUrlFilteringService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
SupervisedUserUrlFilteringServiceFactory*
SupervisedUserUrlFilteringServiceFactory::GetInstance() {
  static base::NoDestructor<SupervisedUserUrlFilteringServiceFactory> instance;
  return instance.get();
}

SupervisedUserUrlFilteringServiceFactory::
    SupervisedUserUrlFilteringServiceFactory()
    : ProfileKeyedServiceFactory("SupervisedUserUrlFilteringService",
                                 BuildProfileSelectionsForRegularAndGuest()) {
  // Temporary dependency on the SupervisedUserService instance to allow
  // migration of legacy SupervisedUserURLFilter methods that are called through
  // the SupervisedUserService: service_->GetURLFilter()->Method(). Remove once
  // all callers are migrated to the SupervisedUserUrlFilteringService.
  // TODO(crbug.com/469336110): Remove this dependency after migration.
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}

SupervisedUserUrlFilteringServiceFactory::
    ~SupervisedUserUrlFilteringServiceFactory() = default;

std::unique_ptr<KeyedService>
SupervisedUserUrlFilteringServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SupervisedUserUrlFilteringService>(
      *SupervisedUserServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

}  // namespace supervised_user
