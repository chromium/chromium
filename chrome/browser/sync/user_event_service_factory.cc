// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/user_event_service_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/no_op_user_event_service.h"
#include "components/sync_user_events/user_event_service_impl.h"
#include "components/sync_user_events/user_event_sync_bridge.h"

namespace browser_sync {

// static
UserEventServiceFactory* UserEventServiceFactory::GetInstance() {
  static base::NoDestructor<UserEventServiceFactory> instance;
  return instance.get();
}

// static
syncer::UserEventService* UserEventServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::UserEventService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEventServiceFactory::UserEventServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserEventService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

UserEventServiceFactory::~UserEventServiceFactory() = default;

KeyedService* UserEventServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return new syncer::NoOpUserEventService();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::USER_EVENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto bridge = std::make_unique<syncer::UserEventSyncBridge>(
      std::move(store_factory), std::move(change_processor),
      SessionSyncServiceFactory::GetForProfile(profile)->GetGlobalIdMapper());
  return new syncer::UserEventServiceImpl(std::move(bridge));
}

}  // namespace browser_sync
