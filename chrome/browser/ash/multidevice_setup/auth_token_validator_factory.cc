// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/auth_token_validator_factory.h"

#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/multidevice_setup/auth_token_validator_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace multidevice_setup {

// static
AuthTokenValidator* AuthTokenValidatorFactory::GetForProfile(Profile* profile) {
  return static_cast<AuthTokenValidatorImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AuthTokenValidatorFactory* AuthTokenValidatorFactory::GetInstance() {
  return base::Singleton<AuthTokenValidatorFactory>::get();
}

AuthTokenValidatorFactory::AuthTokenValidatorFactory()
    : ProfileKeyedServiceFactory("AuthTokenValidatorFactory") {}

AuthTokenValidatorFactory::~AuthTokenValidatorFactory() {}

KeyedService* AuthTokenValidatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AuthTokenValidatorImpl(
      quick_unlock::QuickUnlockFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

}  // namespace multidevice_setup
}  // namespace ash
