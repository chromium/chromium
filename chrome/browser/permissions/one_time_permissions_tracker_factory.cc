// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/profiles/profile.h"

OneTimePermissionsTracker*
OneTimePermissionsTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<OneTimePermissionsTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

OneTimePermissionsTrackerFactory*
OneTimePermissionsTrackerFactory::GetInstance() {
  static base::NoDestructor<OneTimePermissionsTrackerFactory> instance;
  return instance.get();
}

OneTimePermissionsTrackerFactory::OneTimePermissionsTrackerFactory()
    : ProfileKeyedServiceFactory(
          "OneTimePermissionsTrackerKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

OneTimePermissionsTrackerFactory::~OneTimePermissionsTrackerFactory() = default;

bool OneTimePermissionsTrackerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
OneTimePermissionsTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<OneTimePermissionsTracker>();
}
