// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "content/public/common/content_features.h"

/* static */
DIPSService* DIPSServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSServiceFactory* DIPSServiceFactory::GetInstance() {
  static base::NoDestructor<DIPSServiceFactory> instance;
  return instance.get();
}

/* static */
ProfileSelections DIPSServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(features::kDIPS)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return GetHumanProfileSelections();
}

DIPSServiceFactory::DIPSServiceFactory()
    : ProfileKeyedServiceFactory("DIPSService", CreateProfileSelections()) {
  DependsOn(CookieSettingsFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DIPSServiceFactory::~DIPSServiceFactory() = default;

KeyedService* DIPSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSService(context);
}
