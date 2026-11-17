// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/launchpad/launchpad_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/launchpad/features.h"
#include "chrome/browser/launchpad/launchpad_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

// static
launchpad::LaunchpadService* LaunchpadServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<launchpad::LaunchpadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LaunchpadServiceFactory* LaunchpadServiceFactory::GetInstance() {
  static base::NoDestructor<LaunchpadServiceFactory> instance;
  return instance.get();
}

LaunchpadServiceFactory::LaunchpadServiceFactory()
    : ProfileKeyedServiceFactory(
          "LaunchpadService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

LaunchpadServiceFactory::~LaunchpadServiceFactory() = default;

std::unique_ptr<KeyedService>
LaunchpadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(launchpad::features::kLaunchpad)) {
    return nullptr;
  }
  return std::make_unique<launchpad::LaunchpadService>();
}
