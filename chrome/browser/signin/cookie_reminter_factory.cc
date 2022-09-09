// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cookie_reminter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/core/browser/cookie_reminter.h"

CookieReminterFactory::CookieReminterFactory()
    : ProfileKeyedServiceFactory("CookieReminter") {
  DependsOn(IdentityManagerFactory::GetInstance());
}

CookieReminterFactory::~CookieReminterFactory() {}

// static
CookieReminter* CookieReminterFactory::GetForProfile(Profile* profile) {
  return static_cast<CookieReminter*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CookieReminterFactory* CookieReminterFactory::GetInstance() {
  return base::Singleton<CookieReminterFactory>::get();
}

KeyedService* CookieReminterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new CookieReminter(identity_manager);
}
