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
  return base::Singleton<NotifierStateTrackerFactory>::get();
}

NotifierStateTrackerFactory::NotifierStateTrackerFactory()
    : ProfileKeyedServiceFactory(
          "NotifierStateTracker",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(PermissionManagerFactory::GetInstance());
}

NotifierStateTrackerFactory::~NotifierStateTrackerFactory() {}

KeyedService* NotifierStateTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new NotifierStateTracker(static_cast<Profile*>(profile));
}
