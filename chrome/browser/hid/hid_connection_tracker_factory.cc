// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_connection_tracker_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/profiles/profile.h"

// static
HidConnectionTrackerFactory* HidConnectionTrackerFactory::GetInstance() {
  static base::NoDestructor<HidConnectionTrackerFactory> factory;
  return factory.get();
}

// static
HidConnectionTracker* HidConnectionTrackerFactory::GetForProfile(
    Profile* profile,
    bool create) {
  return static_cast<HidConnectionTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, create));
}

HidConnectionTrackerFactory::HidConnectionTrackerFactory()
    : ProfileKeyedServiceFactory(
          "HidConnectionTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

HidConnectionTrackerFactory::~HidConnectionTrackerFactory() = default;

KeyedService* HidConnectionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new HidConnectionTracker(Profile::FromBrowserContext(context));
}

void HidConnectionTrackerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  DCHECK(context);
  auto* hid_connection_tracker =
      GetForProfile(Profile::FromBrowserContext(context), /*create=*/false);
  if (hid_connection_tracker)
    hid_connection_tracker->CleanUp();
  ProfileKeyedServiceFactory::BrowserContextShutdown(context);
}
