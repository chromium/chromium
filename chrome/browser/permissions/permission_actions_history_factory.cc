// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_actions_history_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
    : BrowserContextKeyedServiceFactory(
          "PermissionActionsHistory",
          BrowserContextDependencyManager::GetInstance()) {}

PermissionActionsHistoryFactory::~PermissionActionsHistoryFactory() = default;

KeyedService* PermissionActionsHistoryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::PermissionActionsHistory(profile->GetPrefs());
}

content::BrowserContext*
PermissionActionsHistoryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
