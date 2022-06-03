// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"

namespace badging {

// static
BadgeManager* BadgeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<badging::BadgeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BadgeManagerFactory* BadgeManagerFactory::GetInstance() {
  return base::Singleton<BadgeManagerFactory>::get();
}

BadgeManagerFactory::BadgeManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "BadgeManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

BadgeManagerFactory::~BadgeManagerFactory() {}

KeyedService* BadgeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BadgeManager(Profile::FromBrowserContext(context));
}

}  // namespace badging
