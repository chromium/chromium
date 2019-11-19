// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_SERVICE_FACTORY_HELPER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_SERVICE_FACTORY_HELPER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace notifications {

class DisplayAgent;
class NotificationBackgroundTaskScheduler;
class NotificationSchedulerClientRegistrar;

// Creates the notification schedule service with all the embedder level
// dependencies. This layer is mainly to forbid the embedder to depend on
// notification scheduler internal code.
KeyedService* CreateNotificationScheduleService(
    std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
    std::unique_ptr<NotificationBackgroundTaskScheduler>
        background_task_scheduler,
    std::unique_ptr<DisplayAgent> display_agent,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir,
    bool off_the_record);

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_SCHEDULE_SERVICE_FACTORY_HELPER_H_
