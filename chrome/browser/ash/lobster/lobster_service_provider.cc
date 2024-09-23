// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_service_provider.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "components/manta/manta_service.h"
#include "components/manta/snapper_provider.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"

LobsterService* LobsterServiceProvider::GetForProfile(Profile* profile) {
  return static_cast<LobsterService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

LobsterServiceProvider* LobsterServiceProvider::GetInstance() {
  static base::NoDestructor<LobsterServiceProvider> instance;
  return instance.get();
}

LobsterServiceProvider::LobsterServiceProvider()
    : ProfileKeyedServiceFactory(
          "LobsterServiceProvider",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(manta::MantaServiceFactory::GetInstance());
}

LobsterServiceProvider::~LobsterServiceProvider() = default;

std::unique_ptr<KeyedService> LobsterServiceProvider::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<manta::SnapperProvider> snapper_provider =
      manta::MantaServiceFactory::GetForProfile(profile)
          ->CreateSnapperProvider();
  return std::make_unique<LobsterService>(std::move(snapper_provider), profile);
}

std::unique_ptr<KeyedService>
LobsterServiceProvider::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}
