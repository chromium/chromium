// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifier_state_tracker_factory.h"

#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"

// static
NotifierStateTracker*
NotifierStateTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<NotifierStateTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
NotifierStateTrackerFactory*
NotifierStateTrackerFactory::GetInstance() {
  static base::NoDestructor<NotifierStateTrackerFactory> instance;
  return instance.get();
}

NotifierStateTrackerFactory::NotifierStateTrackerFactory()
    : ProfileKeyedServiceFactory(
          "NotifierStateTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PermissionManagerFactory::GetInstance());
}

NotifierStateTrackerFactory::~NotifierStateTrackerFactory() = default;

std::unique_ptr<KeyedService>
NotifierStateTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  return std::make_unique<NotifierStateTracker>(static_cast<Profile*>(profile));
}
