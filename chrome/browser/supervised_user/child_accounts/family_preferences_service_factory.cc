// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/family_preferences_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/supervised_user/core/browser/family_preferences_service.h"
#include "content/public/browser/browser_context.h"

namespace supervised_user {
// static
FamilyPreferencesService* FamilyPreferencesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<FamilyPreferencesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
FamilyPreferencesServiceFactory*
FamilyPreferencesServiceFactory::GetInstance() {
  static base::NoDestructor<FamilyPreferencesServiceFactory> instance;
  return instance.get();
}

FamilyPreferencesServiceFactory::FamilyPreferencesServiceFactory()
    : ProfileKeyedServiceFactory(
          "FamilyPreferencesService",
          supervised_user::BuildProfileSelectionsForRegularAndGuest()) {}

FamilyPreferencesServiceFactory::~FamilyPreferencesServiceFactory() = default;

KeyedService* FamilyPreferencesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FamilyPreferencesService(
      static_cast<Profile*>(context)->GetPrefs());
}

}  // namespace supervised_user
