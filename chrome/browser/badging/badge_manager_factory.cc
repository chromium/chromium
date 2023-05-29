// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/prefs/pref_service.h"

namespace badging {

// static
BadgeManager* BadgeManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<badging::BadgeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BadgeManagerFactory* BadgeManagerFactory::GetInstance() {
  static base::NoDestructor<BadgeManagerFactory> instance;
  return instance.get();
}

BadgeManagerFactory::BadgeManagerFactory()
    : ProfileKeyedServiceFactory(
          "BadgeManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
}

BadgeManagerFactory::~BadgeManagerFactory() = default;

KeyedService* BadgeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BadgeManager(Profile::FromBrowserContext(context));
}

}  // namespace badging
