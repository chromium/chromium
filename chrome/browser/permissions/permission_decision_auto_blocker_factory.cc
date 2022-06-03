// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_decision_auto_blocker.h"

// static
permissions::PermissionDecisionAutoBlocker*
PermissionDecisionAutoBlockerFactory::GetForProfile(Profile* profile) {
  return static_cast<permissions::PermissionDecisionAutoBlocker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionDecisionAutoBlockerFactory*
PermissionDecisionAutoBlockerFactory::GetInstance() {
  return base::Singleton<PermissionDecisionAutoBlockerFactory>::get();
}

PermissionDecisionAutoBlockerFactory::PermissionDecisionAutoBlockerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionDecisionAutoBlocker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionDecisionAutoBlockerFactory::~PermissionDecisionAutoBlockerFactory() =
    default;

KeyedService* PermissionDecisionAutoBlockerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new permissions::PermissionDecisionAutoBlocker(
      HostContentSettingsMapFactory::GetForProfile(profile));
}

content::BrowserContext*
PermissionDecisionAutoBlockerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
