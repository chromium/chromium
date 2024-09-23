// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"

#include <memory>

#include "chrome/browser/enterprise/identifiers/profile_id_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"

namespace enterprise {

// static
ProfileIdService* ProfileIdServiceFactory::GetForProfile(Profile* profile) {
  if (profile->IsGuestSession() || profile->IsOffTheRecord()) {
    return nullptr;
  }

  return static_cast<ProfileIdService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileIdServiceFactory* ProfileIdServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileIdServiceFactory> instance;
  return instance.get();
}

ProfileIdServiceFactory::ProfileIdServiceFactory()
    : ProfileKeyedServiceFactory(
          "ProfileIdService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ProfileIdServiceFactory::~ProfileIdServiceFactory() = default;

std::unique_ptr<KeyedService>
ProfileIdServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  return std::make_unique<ProfileIdService>(
      std::make_unique<ProfileIdDelegateImpl>(profile), profile->GetPrefs());
}

}  // namespace enterprise
