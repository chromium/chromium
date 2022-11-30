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
  return base::Singleton<PermissionActionsHistoryFactory>::get();
}

PermissionActionsHistoryFactory::PermissionActionsHistoryFactory()
    : ProfileKeyedServiceFactory(
          "PermissionActionsHistory",
          ProfileSelections::BuildForRegularAndIncognito()) {}

PermissionActionsHistoryFactory::~PermissionActionsHistoryFactory() = default;

KeyedService* PermissionActionsHistoryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::PermissionActionsHistory(profile->GetPrefs());
}
