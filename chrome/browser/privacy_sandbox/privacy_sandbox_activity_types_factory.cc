// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_activity_types_factory.h"

#include "chrome/browser/profiles/profile.h"

PrivacySandboxActivityTypesFactory*
PrivacySandboxActivityTypesFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxActivityTypesFactory> instance;
  return instance.get();
}

privacy_sandbox::PrivacySandboxActivityTypesService*
PrivacySandboxActivityTypesFactory::GetForProfile(Profile* profile) {
  return static_cast<privacy_sandbox::PrivacySandboxActivityTypesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PrivacySandboxActivityTypesFactory::PrivacySandboxActivityTypesFactory()
    : ProfileKeyedServiceFactory("PrivacySandboxActivityTypesService") {}

std::unique_ptr<KeyedService>
PrivacySandboxActivityTypesFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<privacy_sandbox::PrivacySandboxActivityTypesService>(
      profile->GetPrefs());
}
