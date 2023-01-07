// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_verifier_factory.h"

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
PasswordSyncTokenVerifierFactory*
PasswordSyncTokenVerifierFactory::GetInstance() {
  return base::Singleton<PasswordSyncTokenVerifierFactory>::get();
}

// static
PasswordSyncTokenVerifier* PasswordSyncTokenVerifierFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PasswordSyncTokenVerifier*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordSyncTokenVerifierFactory::PasswordSyncTokenVerifierFactory()
    : ProfileKeyedServiceFactory("PasswordSyncTokenVerifier") {
  DependsOn(InSessionPasswordSyncManagerFactory::GetInstance());
}

PasswordSyncTokenVerifierFactory::~PasswordSyncTokenVerifierFactory() = default;

KeyedService* PasswordSyncTokenVerifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);

  // PasswordSyncTokenVerifier should be created for the primary user only.
  if (!ProfileHelper::IsPrimaryProfile(profile) || !user ||
      !user->using_saml()) {
    return nullptr;
  }
  return new PasswordSyncTokenVerifier(profile);
}

}  // namespace ash
