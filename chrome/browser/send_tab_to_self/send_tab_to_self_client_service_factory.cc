// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"

#include <string>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif

namespace send_tab_to_self {
// static
SendTabToSelfClientService* SendTabToSelfClientServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<send_tab_to_self::SendTabToSelfClientService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SendTabToSelfClientServiceFactory*
SendTabToSelfClientServiceFactory::GetInstance() {
  return base::Singleton<SendTabToSelfClientServiceFactory>::get();
}

SendTabToSelfClientServiceFactory::SendTabToSelfClientServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SendTabToSelfClientService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
}

SendTabToSelfClientServiceFactory::~SendTabToSelfClientServiceFactory() {}

// BrowserStateKeyedServiceFactory implementation.
KeyedService* SendTabToSelfClientServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  SendTabToSelfSyncService* sync_service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);

#if defined(OS_CHROMEOS)
  // Create SendTabToSelfClientService only for profiles of Gaia users.
  // ChromeOS has system level profiles, such as the sign-in profile, or
  // users that are not Gaia users, such as public account users. Do not
  // create the service for them.
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  // Ensure that the profile is a user profile.
  if (!user)
    return nullptr;
  // Ensure that the user is a Gaia user, since other types of user should not
  // have access to the service.
  if (!user->HasGaiaAccount())
    return nullptr;
#endif

  // TODO(crbug.com/976741) refactor profile out of STTSClient constructor.
  return new SendTabToSelfClientService(profile,
                                        sync_service->GetSendTabToSelfModel());
}

bool SendTabToSelfClientServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SendTabToSelfClientServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace send_tab_to_self
