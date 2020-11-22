// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reading_list/android/reading_list_notification_service.h"

#include "base/notreached.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "components/reading_list/core/reading_list_model.h"

ReadingListNotificationService::ReadingListNotificationService(
    ReadingListModel* reading_list_model,
    notifications::NotificationScheduleService* notification_scheduler)
    : reading_list_model_(reading_list_model),
      notification_scheduler_(notification_scheduler) {
  DCHECK(reading_list_model_);
  DCHECK(notification_scheduler_);
  NOTIMPLEMENTED();
}
