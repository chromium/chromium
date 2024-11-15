// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

PasswordChangeServiceFactory::PasswordChangeServiceFactory()
    : ProfileKeyedServiceFactory("PasswordChangeServiceFactory",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(AffiliationServiceFactory::GetInstance());
}

PasswordChangeServiceFactory::~PasswordChangeServiceFactory() = default;

PasswordChangeServiceFactory* PasswordChangeServiceFactory::GetInstance() {
  static base::NoDestructor<PasswordChangeServiceFactory> instance;
  return instance.get();
}

ChromePasswordChangeService* PasswordChangeServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromePasswordChangeService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
PasswordChangeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<ChromePasswordChangeService>(
      AffiliationServiceFactory::GetForProfile(profile));
}
