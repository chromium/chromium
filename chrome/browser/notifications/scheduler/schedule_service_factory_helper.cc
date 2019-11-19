// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"

#include <utility>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/internal/display_decider.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/internal/impression_store.h"
#include "chrome/browser/notifications/scheduler/internal/init_aware_scheduler.h"
#include "chrome/browser/notifications/scheduler/internal/noop_notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/internal/notification_schedule_service_impl.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/notification_store.h"
#include "chrome/browser/notifications/scheduler/internal/png_icon_converter_impl.h"
#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"
#include "chrome/browser/notifications/scheduler/internal/webui_client.h"
#include "chrome/browser/notifications/scheduler/public/display_agent.h"
#include "chrome/browser/notifications/scheduler/public/features.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace notifications {
namespace {
const base::FilePath::CharType kImpressionDBName[] =
    FILE_PATH_LITERAL("ImpressionDB");
const base::FilePath::CharType kIconDBName[] = FILE_PATH_LITERAL("IconDB");
const base::FilePath::CharType kNotificationDBName[] =
    FILE_PATH_LITERAL("NotificationDB");
}  // namespace

KeyedService* CreateNotificationScheduleService(
    std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
    std::unique_ptr<NotificationBackgroundTaskScheduler>
        background_task_scheduler,
    std::unique_ptr<DisplayAgent> display_agent,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    bool off_the_record) {
  if (!base::FeatureList::IsEnabled(features::kNotificationScheduleService) ||
      off_the_record)
    return static_cast<KeyedService*>(new NoopNotificationScheduleService());

  auto config = SchedulerConfig::Create();
  auto task_runner = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  client_registrar->RegisterClient(SchedulerClientType::kWebUI,
                                   std::make_unique<WebUIClient>());

  // Build icon store.
  base::FilePath icon_store_dir = storage_dir.Append(kIconDBName);
  auto icon_db = db_provider->GetDB<proto::Icon, IconEntry>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_ICON_STORE,
      icon_store_dir, task_runner);
  auto icon_store = std::make_unique<IconProtoDbStore>(
      std::move(icon_db), std::make_unique<PngIconConverterImpl>());

  // Build impression tracker.
  base::FilePath impression_store_dir = storage_dir.Append(kImpressionDBName);
  auto impression_db = db_provider->GetDB<proto::ClientState, ClientState>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_IMPRESSION_STORE,
      impression_store_dir, task_runner);
  auto impression_store =
      std::make_unique<ImpressionStore>(std::move(impression_db));
  std::vector<SchedulerClientType> registered_clients;
  client_registrar->GetRegisteredClients(&registered_clients);
  auto impression_tracker = std::make_unique<ImpressionHistoryTrackerImpl>(
      *config.get(), registered_clients, std::move(impression_store),
      base::DefaultClock::GetInstance());

  // Build scheduled notification manager.
  base::FilePath notification_store_dir =
      storage_dir.Append(kNotificationDBName);
  auto notification_db = db_provider->GetDB<proto::NotificationEntry,
                                            notifications::NotificationEntry>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_NOTIFICATION_STORE,
      notification_store_dir, task_runner);
  auto notification_store =
      std::make_unique<NotificationStore>(std::move(notification_db));

  auto notification_manager = ScheduledNotificationManager::Create(
      std::move(notification_store), std::move(icon_store), registered_clients,
      *config.get());

  auto background_task_coordinator = BackgroundTaskCoordinator::Create(
      std::move(background_task_scheduler), config.get(),
      base::DefaultClock::GetInstance());

  auto display_decider = DisplayDecider::Create(
      config.get(), registered_clients, base::DefaultClock::GetInstance());
  auto context = std::make_unique<NotificationSchedulerContext>(
      std::move(client_registrar), std::move(background_task_coordinator),
      std::move(impression_tracker), std::move(notification_manager),
      std::move(display_agent), std::move(display_decider), std::move(config));

  auto scheduler = NotificationScheduler::Create(std::move(context));
  auto init_aware_scheduler =
      std::make_unique<InitAwareNotificationScheduler>(std::move(scheduler));
  return static_cast<KeyedService*>(
      new NotificationScheduleServiceImpl(std::move(init_aware_scheduler)));
}

}  // namespace notifications
