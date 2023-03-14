// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"

namespace guest_os {

// static
GuestOsSessionTracker* GuestOsSessionTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GuestOsSessionTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsSessionTrackerFactory* GuestOsSessionTrackerFactory::GetInstance() {
  static base::NoDestructor<GuestOsSessionTrackerFactory> factory;
  return factory.get();
}

GuestOsSessionTrackerFactory::GuestOsSessionTrackerFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsSessionTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

GuestOsSessionTrackerFactory::~GuestOsSessionTrackerFactory() = default;

KeyedService* GuestOsSessionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsSessionTracker(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile));
}

}  // namespace guest_os
