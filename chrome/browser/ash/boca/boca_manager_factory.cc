// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/boca/boca_role_util.h"

namespace ash {
// static
BocaManagerFactory* BocaManagerFactory::GetInstance() {
  static base::NoDestructor<BocaManagerFactory> instance;
  return instance.get();
}

// static
BocaManager* BocaManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<BocaManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BocaManagerFactory::BocaManagerFactory()
    : ProfileKeyedServiceFactory(
          "BocaManagerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // Do not init for ash internal such as login and lock screen.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

BocaManagerFactory::~BocaManagerFactory() = default;

std::unique_ptr<KeyedService>
BocaManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(boca_util::IsEnabled());
  Profile* profile = Profile::FromBrowserContext(context);
  auto service = std::make_unique<BocaManager>(profile);
  return service;
}

}  // namespace ash
