// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/signin_bridge_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"

// static
SigninBridge* SigninBridgeFactory::GetForProfile(Profile* profile) {
  return static_cast<SigninBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SigninBridgeFactory* SigninBridgeFactory::GetInstance() {
  static base::NoDestructor<SigninBridgeFactory> instance;
  return instance.get();
}

SigninBridgeFactory::SigninBridgeFactory()
    : ProfileKeyedServiceFactory(
          "SigninBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

std::unique_ptr<KeyedService>
SigninBridgeFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SigninBridge>();
}
