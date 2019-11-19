// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/public/impression_detail.h"

namespace notifications {

ImpressionDetail::ImpressionDetail()
    : current_max_daily_show(0), num_shown_today(0) {}

ImpressionDetail::ImpressionDetail(int current_max_daily_show,
                                   int num_shown_today)
    : current_max_daily_show(current_max_daily_show),
      num_shown_today(num_shown_today) {}

ImpressionDetail::ImpressionDetail(const ImpressionDetail& other) = default;

ImpressionDetail::~ImpressionDetail() = default;

bool ImpressionDetail::operator==(const ImpressionDetail& other) const {
  return current_max_daily_show == other.current_max_daily_show &&
         num_shown_today == other.num_shown_today;
}

}  // namespace notifications
