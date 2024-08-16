// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_receiver_service_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"

// static
PasswordReceiverServiceFactory* PasswordReceiverServiceFactory::GetInstance() {
  static base::NoDestructor<PasswordReceiverServiceFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordReceiverService*
PasswordReceiverServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::PasswordReceiverService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordReceiverServiceFactory::PasswordReceiverServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasswordReceiverService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
}

PasswordReceiverServiceFactory::~PasswordReceiverServiceFactory() = default;

std::unique_ptr<KeyedService>
PasswordReceiverServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
// Password receiving on Android is handled in GMSCore, and hence no service
// should be instantiated.
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else

  Profile* profile = Profile::FromBrowserContext(context);

  // Since Password Manager doesn't work for non-standard profiles, the
  // PasswordReceiverService also shouldn't be created for such profiles.
  CHECK(!profile->IsOffTheRecord());
  CHECK(profile->IsRegularProfile());

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::INCOMING_PASSWORD_SHARING_INVITATION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto sync_bridge = std::make_unique<
      password_manager::IncomingPasswordSharingInvitationSyncBridge>(
      std::move(change_processor),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());

  return std::make_unique<password_manager::PasswordReceiverServiceImpl>(
      profile->GetPrefs(), std::move(sync_bridge),
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
#endif  // BUILDFLAG(IS_ANDROID)
}
