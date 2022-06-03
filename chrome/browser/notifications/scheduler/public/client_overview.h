// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_

#include "base/callback.h"

#include "chrome/browser/notifications/scheduler/public/impression_detail.h"

namespace notifications {

struct ClientOverview {
  using ClientOverviewCallback = base::OnceCallback<void(ClientOverview)>;

  ClientOverview();
  ClientOverview(ImpressionDetail impression_detail,
                 size_t num_scheduled_notifications);
  ClientOverview(const ClientOverview& other);
  ~ClientOverview();
  bool operator==(const ClientOverview& other) const;

  // Details of impression.
  ImpressionDetail impression_detail;

  // The number of notifications cached in scheduler but not displayed yet.
  size_t num_scheduled_notifications;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_CLIENT_OVERVIEW_H_
