// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/account_setting_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"

namespace autofill {

// static
AccountSettingServiceFactory* AccountSettingServiceFactory::GetInstance() {
  static base::NoDestructor<AccountSettingServiceFactory> instance;
  return instance.get();
}

// static
AccountSettingService* AccountSettingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountSettingService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

AccountSettingServiceFactory::AccountSettingServiceFactory()
    : ProfileKeyedServiceFactory(
          "AccountSettingService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

std::unique_ptr<KeyedService>
AccountSettingServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AccountSettingService>();
}

}  // namespace autofill
