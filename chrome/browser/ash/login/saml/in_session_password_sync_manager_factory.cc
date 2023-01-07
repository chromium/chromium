// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
InSessionPasswordSyncManagerFactory*
InSessionPasswordSyncManagerFactory::GetInstance() {
  return base::Singleton<InSessionPasswordSyncManagerFactory>::get();
}

// static
InSessionPasswordSyncManager*
InSessionPasswordSyncManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<InSessionPasswordSyncManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

InSessionPasswordSyncManagerFactory::InSessionPasswordSyncManagerFactory()
    : ProfileKeyedServiceFactory("InSessionPasswordSyncManager") {}

InSessionPasswordSyncManagerFactory::~InSessionPasswordSyncManagerFactory() =
    default;

KeyedService* InSessionPasswordSyncManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // InSessionPasswordSyncManager should be created for the primary user only.
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }
  return new InSessionPasswordSyncManager(profile);
}

}  // namespace ash
