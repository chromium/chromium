// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
InSessionPasswordSyncManagerFactory*
InSessionPasswordSyncManagerFactory::GetInstance() {
  static base::NoDestructor<InSessionPasswordSyncManagerFactory> instance;
  return instance.get();
}

// static
InSessionPasswordSyncManager*
InSessionPasswordSyncManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<InSessionPasswordSyncManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

InSessionPasswordSyncManagerFactory::InSessionPasswordSyncManagerFactory()
    : ProfileKeyedServiceFactory(
          "InSessionPasswordSyncManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

InSessionPasswordSyncManagerFactory::~InSessionPasswordSyncManagerFactory() =
    default;

std::unique_ptr<KeyedService>
InSessionPasswordSyncManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // InSessionPasswordSyncManager should be created for the primary user only.
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }
  return std::make_unique<InSessionPasswordSyncManager>(profile);
}

}  // namespace ash
