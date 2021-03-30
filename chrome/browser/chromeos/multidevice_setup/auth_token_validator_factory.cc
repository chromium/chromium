// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_factory.h"

#include "base/macros.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/auth_token_validator_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

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
    : BrowserContextKeyedServiceFactory(
          "AuthTokenValidatorFactory",
          BrowserContextDependencyManager::GetInstance()) {}

AuthTokenValidatorFactory::~AuthTokenValidatorFactory() {}

KeyedService* AuthTokenValidatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AuthTokenValidatorImpl(
      chromeos::quick_unlock::QuickUnlockFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

}  // namespace multidevice_setup

}  // namespace chromeos
