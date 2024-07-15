// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"

#include <string>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
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
  static base::NoDestructor<SendTabToSelfClientServiceFactory> instance;
  return instance.get();
}

SendTabToSelfClientServiceFactory::SendTabToSelfClientServiceFactory()
    : ProfileKeyedServiceFactory(
          "SendTabToSelfClientService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(NotificationDisplayServiceFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
}

SendTabToSelfClientServiceFactory::~SendTabToSelfClientServiceFactory() =
    default;

// BrowserStateKeyedServiceFactory implementation.
std::unique_ptr<KeyedService>
SendTabToSelfClientServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  SendTabToSelfSyncService* sync_service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Create SendTabToSelfClientService only for profiles of Gaia users.
  // ChromeOS has system level profiles, such as the sign-in profile, or
  // users that are not Gaia users, such as public account users. Do not
  // create the service for them.
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  // Ensure that the profile is a user profile.
  if (!user)
    return nullptr;
  // Ensure that the user is a Gaia user, since other types of user should not
  // have access to the service.
  if (!user->HasGaiaAccount())
    return nullptr;
#endif

  // TODO(crbug.com/40632832) refactor profile out of STTSClient constructor.
  return std::make_unique<SendTabToSelfClientService>(
      profile, sync_service->GetSendTabToSelfModel());
}

bool SendTabToSelfClientServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool SendTabToSelfClientServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace send_tab_to_self
