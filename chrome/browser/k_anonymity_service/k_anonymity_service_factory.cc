// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_factory.h"

#include "build/branding_buildflags.h"
#include "build/build_config.h"

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/k_anonymity_service_delegate.h"

namespace {
ProfileSelections BuildKAnonymityServiceProfileSelections() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::FeatureList::IsEnabled(features::kKAnonymityService))
    return ProfileSelections::BuildNoProfilesSelected();
  return ProfileSelections::BuildForRegularAndIncognito();
#else
  return ProfileSelections::BuildNoProfilesSelected();
#endif
}

}  // namespace

// static
KAnonymityServiceFactory* KAnonymityServiceFactory::GetInstance() {
  static base::NoDestructor<KAnonymityServiceFactory> instance;
  return instance.get();
}

// static
content::KAnonymityServiceDelegate* KAnonymityServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<KAnonymityServiceClient*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 /*create=*/true));
}

KAnonymityServiceFactory::KAnonymityServiceFactory()
    : ProfileKeyedServiceFactory("KAnonymityServiceFactory",
                                 BuildKAnonymityServiceProfileSelections()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

KAnonymityServiceFactory::~KAnonymityServiceFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* KAnonymityServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new KAnonymityServiceClient(Profile::FromBrowserContext(context));
}
