// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"

#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
LockScreenReauthManagerFactory* LockScreenReauthManagerFactory::GetInstance() {
  static base::NoDestructor<LockScreenReauthManagerFactory> instance;
  return instance.get();
}

// static
LockScreenReauthManager* LockScreenReauthManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LockScreenReauthManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

LockScreenReauthManagerFactory::LockScreenReauthManagerFactory()
    : ProfileKeyedServiceFactory(
          "LockScreenReauthManager",
          ProfileSelections::Builder()
              // Works only with regular profiles and not off the record (OTR).
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

LockScreenReauthManagerFactory::~LockScreenReauthManagerFactory() = default;

std::unique_ptr<KeyedService>
LockScreenReauthManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // LockScreenReauthManager should be created for the primary user only.
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }
  return std::make_unique<LockScreenReauthManager>(profile);
}

}  // namespace ash
