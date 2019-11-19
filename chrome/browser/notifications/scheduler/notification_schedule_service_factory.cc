// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_ANDROID)
#include "chrome/browser/notifications/scheduler/display_agent_android.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_android.h"
#include "chrome/browser/updates/update_notification_client.h"
#endif  // defined(OS_ANDROID)

namespace {
std::unique_ptr<notifications::NotificationSchedulerClientRegistrar>
RegisterClients() {
  auto client_registrar =
      std::make_unique<notifications::NotificationSchedulerClientRegistrar>();
  // TODO(xingliu): Register clients here.
#if defined(OS_ANDROID)
  // Register UpdateNotificationClient.
  auto chrome_update_client =
      std::make_unique<updates::UpdateNotificationClient>();
  client_registrar->RegisterClient(
      notifications::SchedulerClientType::kChromeUpdate,
      std::move(chrome_update_client));
#endif  // defined(OS_ANDROID)
  return client_registrar;
}

}  // namespace

// static
NotificationScheduleServiceFactory*
NotificationScheduleServiceFactory::GetInstance() {
  static base::NoDestructor<NotificationScheduleServiceFactory> instance;
  return instance.get();
}

// static
notifications::NotificationScheduleService*
NotificationScheduleServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<notifications::NotificationScheduleService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

NotificationScheduleServiceFactory::NotificationScheduleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "notifications::NotificationScheduleService",
          BrowserContextDependencyManager::GetInstance()) {}

NotificationScheduleServiceFactory::~NotificationScheduleServiceFactory() =
    default;

KeyedService* NotificationScheduleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  base::FilePath storage_dir =
      profile->GetPath().Append(chrome::kNotificationSchedulerStorageDirname);
  auto client_registrar = RegisterClients();
#if defined(OS_ANDROID)
  auto display_agent = std::make_unique<DisplayAgentAndroid>();
  auto background_task_scheduler =
      std::make_unique<NotificationBackgroundTaskSchedulerAndroid>();
#else
  auto display_agent = notifications::DisplayAgent::Create();
  auto background_task_scheduler =
      std::make_unique<NotificationBackgroundTaskSchedulerImpl>();
#endif  // defined(OS_ANDROID)
  auto* db_provider =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetProtoDatabaseProvider();
  return notifications::CreateNotificationScheduleService(
      std::move(client_registrar), std::move(background_task_scheduler),
      std::move(display_agent), db_provider, storage_dir,
      context->IsOffTheRecord());
}

content::BrowserContext*
NotificationScheduleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Separate incognito instance that does nothing.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
