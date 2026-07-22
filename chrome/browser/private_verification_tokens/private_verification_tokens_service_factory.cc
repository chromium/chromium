// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "net/base/features.h"

PrivateVerificationTokensService*
PrivateVerificationTokensServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<PrivateVerificationTokensService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

PrivateVerificationTokensServiceFactory*
PrivateVerificationTokensServiceFactory::GetInstance() {
  static base::NoDestructor<PrivateVerificationTokensServiceFactory> instance;
  return instance.get();
}

ProfileSelections
PrivateVerificationTokensServiceFactory::CreateProfileSelections() {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kNone)
      .WithSystem(ProfileSelection::kNone)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

std::unique_ptr<KeyedService>
PrivateVerificationTokensServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          net::features::kEnablePrivateVerificationTokens)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);
  return PrivateVerificationTokensService::Create(
      profile->GetPath(),
      HostContentSettingsMapFactory::GetForProfile(profile));
}

bool PrivateVerificationTokensServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return false;
}

PrivateVerificationTokensServiceFactory::
    PrivateVerificationTokensServiceFactory()
    : ProfileKeyedServiceFactory("PrivateVerificationTokensServiceFactory",
                                 CreateProfileSelections()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PrivateVerificationTokensServiceFactory::
    ~PrivateVerificationTokensServiceFactory() = default;
