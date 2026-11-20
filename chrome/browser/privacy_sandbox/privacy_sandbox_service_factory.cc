// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

PrivacySandboxServiceFactory* PrivacySandboxServiceFactory::GetInstance() {
  static base::NoDestructor<PrivacySandboxServiceFactory> instance;
  return instance.get();
}

PrivacySandboxService* PrivacySandboxServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrivacySandboxService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrivacySandboxServiceFactory::PrivacySandboxServiceFactory()
    : ProfileKeyedServiceFactory(
          "PrivacySandboxService",
          // TODO(crbug.com/40814288): Determine whether this actually needs to
          // be created, or whether all usage in OTR contexts can be removed.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

std::unique_ptr<KeyedService>
PrivacySandboxServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PrivacySandboxServiceImpl>(profile->GetPrefs());
}
