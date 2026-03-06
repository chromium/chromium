// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_

#include "base/functional/callback.h"
#include "chrome/browser/notifications/scheduler/public/impression_detail.h"
#include "chrome/browser/notifications/scheduler/public/notification_entry.h"

namespace notifications {

struct ClientOverview {
  using ClientOverviewCallback = base::OnceCallback<void(ClientOverview)>;

  ClientOverview();
  ClientOverview(ImpressionDetail impression_detail,
                 std::vector<const NotificationEntry*> scheduled_notifications);
  ClientOverview(const ClientOverview& other);
  ClientOverview(ClientOverview&& other);
  ClientOverview& operator=(const ClientOverview& other);
  ClientOverview& operator=(ClientOverview&& other);
  ~ClientOverview();

  bool operator==(const ClientOverview& other) const;

  // Details of impression.
  ImpressionDetail impression_detail;

  // A list of notifications cached in the scheduler but not displayed yet.
  std::vector<const NotificationEntry*> scheduled_notifications;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_
