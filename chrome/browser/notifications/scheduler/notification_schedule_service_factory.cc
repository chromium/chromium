// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

namespace {

std::unique_ptr<notifications::NotificationSchedulerClientRegistrar>
RegisterClients(ProfileKey* key) {
  auto client_registrar =
      std::make_unique<notifications::NotificationSchedulerClientRegistrar>();
  return client_registrar;
}

}  // namespace

// static
NotificationScheduleServiceFactory*
NotificationScheduleServiceFactory::GetInstance() {
  return base::Singleton<NotificationScheduleServiceFactory>::get();
}

// static
notifications::NotificationScheduleService*
NotificationScheduleServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<notifications::NotificationScheduleService*>(
      GetInstance()->GetServiceForKey(key, true /* create */));
}

NotificationScheduleServiceFactory::NotificationScheduleServiceFactory()
    : SimpleKeyedServiceFactory("notifications::NotificationScheduleService",
                                SimpleDependencyManager::GetInstance()) {}

NotificationScheduleServiceFactory::~NotificationScheduleServiceFactory() =
    default;

std::unique_ptr<KeyedService>
NotificationScheduleServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  base::FilePath storage_dir = profile_key->GetPath().Append(
      chrome::kNotificationSchedulerStorageDirname);
  auto client_registrar = RegisterClients(profile_key);
#if BUILDFLAG(IS_ANDROID)
  auto display_agent = nullptr;
  auto background_task_scheduler = nullptr;
#else
  auto display_agent = notifications::DisplayAgent::Create();
  auto background_task_scheduler =
      std::make_unique<NotificationBackgroundTaskSchedulerImpl>();
#endif  // BUILDFLAG(IS_ANDROID)
  auto* db_provider = profile_key->GetProtoDatabaseProvider();
  return notifications::CreateNotificationScheduleService(
      std::move(client_registrar), std::move(background_task_scheduler),
      std::move(display_agent), db_provider, storage_dir,
      profile_key->IsOffTheRecord());
}

SimpleFactoryKey* NotificationScheduleServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  // Separate incognito instance that does nothing.
  ProfileKey* profile_key = ProfileKey::FromSimpleFactoryKey(key);
  return profile_key->GetOriginalKey();
}
