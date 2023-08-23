// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_sender_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_sync_bridge.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service_impl.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"

// static
PasswordSenderServiceFactory* PasswordSenderServiceFactory::GetInstance() {
  static base::NoDestructor<PasswordSenderServiceFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordSenderService*
PasswordSenderServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::PasswordSenderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordSenderServiceFactory::PasswordSenderServiceFactory()
    : ProfileKeyedServiceFactory(
          "PasswordSenderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

PasswordSenderServiceFactory::~PasswordSenderServiceFactory() = default;

std::unique_ptr<KeyedService>
PasswordSenderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManagerEnableSenderService)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  // Since Password Manager doesn't work for non-standard profiles, the
  // PasswordSenderService also shouldn't be created for such profiles.
  CHECK(!profile->IsOffTheRecord());
  CHECK(profile->IsRegularProfile());

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::OUTGOING_PASSWORD_SHARING_INVITATION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));

  auto sync_bridge = std::make_unique<
      password_manager::OutgoingPasswordSharingInvitationSyncBridge>(
      std::move(change_processor));

  return std::make_unique<password_manager::PasswordSenderServiceImpl>(
      std::move(sync_bridge));
}
