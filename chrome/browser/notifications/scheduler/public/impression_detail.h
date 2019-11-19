// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_

#include "base/callback.h"

namespace notifications {

struct ImpressionDetail {
  using ImpressionDetailCallback = base::OnceCallback<void(ImpressionDetail)>;

  ImpressionDetail();
  ImpressionDetail(int current_max_daily_show, int num_shown_today);
  ImpressionDetail(const ImpressionDetail& other);
  ~ImpressionDetail();
  bool operator==(const ImpressionDetail& other) const;

  // The maximum number of notifications shown to the user for this type.
  // May change if the user interacts with the notification per day.
  int current_max_daily_show;

  // The current number of notifications shown to the user for this type today.
  int num_shown_today;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_IMPRESSION_DETAIL_H_
