// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_signin_manager_builder.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"

std::unique_ptr<KeyedService> BuildFakeSigninManagerForTesting(
    content::BrowserContext* context) {
  std::unique_ptr<SigninManagerBase> manager;
  Profile* profile = static_cast<Profile*>(context);
  manager.reset(new FakeSigninManagerForTesting(profile));
  manager->Initialize(nullptr);
  SigninManagerFactory::GetInstance()
      ->NotifyObserversOfSigninManagerCreationForTesting(manager.get());
  return std::move(manager);
}

#if defined(OS_CHROMEOS)
FakeSigninManagerForTesting::FakeSigninManagerForTesting(Profile* profile)
    : FakeSigninManagerBase(
          ChromeSigninClientFactory::GetForProfile(profile),
          AccountTrackerServiceFactory::GetForProfile(profile),
          SigninErrorControllerFactory::GetForProfile(profile)) {}
#else
FakeSigninManagerForTesting::FakeSigninManagerForTesting(Profile* profile)
    : FakeSigninManager(
          ChromeSigninClientFactory::GetForProfile(profile),
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
          AccountTrackerServiceFactory::GetForProfile(profile),
          GaiaCookieManagerServiceFactory::GetForProfile(profile),
          SigninErrorControllerFactory::GetForProfile(profile)) {}
#endif
