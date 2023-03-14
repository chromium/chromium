// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service.h"

/* static */
DIPSService* DIPSServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSServiceFactory* DIPSServiceFactory::GetInstance() {
  return base::Singleton<DIPSServiceFactory>::get();
}

/* static */
ProfileSelections DIPSServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(dips::kFeature)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return GetHumanProfileSelections();
}

DIPSServiceFactory::DIPSServiceFactory()
    : ProfileKeyedServiceFactory("DIPSService", CreateProfileSelections()) {
  DependsOn(CookieSettingsFactory::GetInstance());
}

DIPSServiceFactory::~DIPSServiceFactory() = default;

KeyedService* DIPSServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSService(context);
}
