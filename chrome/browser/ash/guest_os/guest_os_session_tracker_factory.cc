// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "GuestOsSessionTracker",
          BrowserContextDependencyManager::GetInstance()) {}

GuestOsSessionTrackerFactory::~GuestOsSessionTrackerFactory() = default;

KeyedService* GuestOsSessionTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsSessionTracker(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile));
}

}  // namespace guest_os
