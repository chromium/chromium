// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_receiver_service_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"

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
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
}

PasswordReceiverServiceFactory::~PasswordReceiverServiceFactory() = default;

KeyedService* PasswordReceiverServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerEnableReceiverService)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  // Since Password Manager doesn't work for non-standard profiles, the
  // PasswordReceiverService also shouldn't be created for such profiles.
  CHECK(!profile->IsOffTheRecord());
  CHECK(profile->IsRegularProfile());

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::INCOMING_PASSWORD_SHARING_INVITATION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto sync_bridge = std::make_unique<
      password_manager::IncomingPasswordSharingInvitationSyncBridge>(
      std::move(change_processor),
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());

  // To avoid circular dependency between SyncService and
  // PasswordReceiverService, PasswordReceiverService cannot depend on
  // SyncService and hence during the construction of the
  // PasswordReceiverService, the SyncService hasn't been necessarily
  // constructed yet. Therefore, pass over a repeating callback that will fetch
  // the SyncService on demand. This is expected to be called only after
  // incoming password sharing invitations are downloaded from the sync server,
  // at which time, the SyncService must have been constructed already.
  auto sync_service_getter = base::BindRepeating(
      [](Profile* profile) {
        // Avoid creating the sync service here. Only return it if exists since
        // it isn't necessarily safe to construct it because of the missing
        // dependency.
        return SyncServiceFactory::HasSyncService(profile)
                   ? SyncServiceFactory::GetForProfile(profile)
                   : nullptr;
      },
      profile);

  return new password_manager::PasswordReceiverServiceImpl(
      profile->GetPrefs(), std::move(sync_service_getter),
      std::move(sync_bridge),
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS)
          .get(),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get());
}
