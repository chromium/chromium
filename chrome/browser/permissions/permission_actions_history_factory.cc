// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_actions_history_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_actions_history.h"

// static
permissions::PermissionActionsHistory*
PermissionActionsHistoryFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::PermissionActionsHistory*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionActionsHistoryFactory*
PermissionActionsHistoryFactory::GetInstance() {
  static base::NoDestructor<PermissionActionsHistoryFactory> instance;
  return instance.get();
}

PermissionActionsHistoryFactory::PermissionActionsHistoryFactory()
    : ProfileKeyedServiceFactory(
          "PermissionActionsHistory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

PermissionActionsHistoryFactory::~PermissionActionsHistoryFactory() = default;

std::unique_ptr<KeyedService>
PermissionActionsHistoryFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<permissions::PermissionActionsHistory>(
      profile->GetPrefs());
}
