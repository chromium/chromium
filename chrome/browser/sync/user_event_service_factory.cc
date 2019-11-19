// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/user_event_service_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/no_op_user_event_service.h"
#include "components/sync_user_events/user_event_service_impl.h"
#include "components/sync_user_events/user_event_sync_bridge.h"

namespace browser_sync {

// static
UserEventServiceFactory* UserEventServiceFactory::GetInstance() {
  return base::Singleton<UserEventServiceFactory>::get();
}

// static
syncer::UserEventService* UserEventServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::UserEventService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

UserEventServiceFactory::UserEventServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserEventService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

UserEventServiceFactory::~UserEventServiceFactory() {}

KeyedService* UserEventServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord()) {
    return new syncer::NoOpUserEventService();
  }

  Profile* profile = Profile::FromBrowserContext(context);
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::USER_EVENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto bridge = std::make_unique<syncer::UserEventSyncBridge>(
      std::move(store_factory), std::move(change_processor),
      SessionSyncServiceFactory::GetForProfile(profile)->GetGlobalIdMapper());
  return new syncer::UserEventServiceImpl(std::move(bridge));
}

content::BrowserContext* UserEventServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace browser_sync
