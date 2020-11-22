// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace notifications {
class NotificationScheduleService;
}  // namespace notifications

class ReadingListModel;

class ReadingListNotificationService : public KeyedService {
 public:
  ReadingListNotificationService(
      ReadingListModel* reading_list_model,
      notifications::NotificationScheduleService* notification_scheduler);
  ~ReadingListNotificationService() override = default;

  ReadingListNotificationService(const ReadingListNotificationService&) =
      delete;
  ReadingListNotificationService& operator=(
      const ReadingListNotificationService&) = delete;

 private:
  // Contains reading list data, outlives this class.
  ReadingListModel* reading_list_model_;

  // Used to schedule notification, outlives this class.
  notifications::NotificationScheduleService* notification_scheduler_;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_NOTIFICATION_SERVICE_H_
