// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_verifier_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
PasswordSyncTokenVerifierFactory*
PasswordSyncTokenVerifierFactory::GetInstance() {
  static base::NoDestructor<PasswordSyncTokenVerifierFactory> instance;
  return instance.get();
}

// static
PasswordSyncTokenVerifier* PasswordSyncTokenVerifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PasswordSyncTokenVerifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordSyncTokenVerifierFactory::PasswordSyncTokenVerifierFactory()
    : ProfileKeyedServiceFactory(
          "PasswordSyncTokenVerifier",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

PasswordSyncTokenVerifierFactory::~PasswordSyncTokenVerifierFactory() = default;

std::unique_ptr<KeyedService>
PasswordSyncTokenVerifierFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);

  // PasswordSyncTokenVerifier should be created for the primary user only.
  if (!ProfileHelper::IsPrimaryProfile(profile) || !user ||
      !user->using_saml()) {
    return nullptr;
  }
  return std::make_unique<PasswordSyncTokenVerifier>(profile);
}

}  // namespace ash
