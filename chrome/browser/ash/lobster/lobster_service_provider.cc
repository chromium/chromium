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
  // Exceptionally access g_browser_process as this class lives in
  // base::NoDestructor.
  // NOTE: VariationsService does not outlive a ProfileKeyedService, so we need
  // the callback to avoid a dangling pointer.
  auto variations_service_callback = base::BindRepeating(
      []() { return g_browser_process->variations_service(); });

  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<manta::SnapperProvider> snapper_provider =
      manta::MantaServiceFactory::GetForProfile(profile)
          ->CreateSnapperProvider();
  return std::make_unique<LobsterService>(
      std::move(snapper_provider), profile,
      std::move(variations_service_callback));
}

std::unique_ptr<KeyedService>
LobsterServiceProvider::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}
