// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/profiles/profile.h"

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
              .Build()) {}

BocaManagerFactory::~BocaManagerFactory() = default;

std::unique_ptr<KeyedService>
BocaManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<BocaManager>(profile);
}

}  // namespace ash
