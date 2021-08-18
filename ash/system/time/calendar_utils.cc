// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

namespace ash {

bool IsToday(const base::Time::Exploded& selected_date) {
  base::Time::Exploded today_exploded = GetExploded(base::Time::Now());
  return selected_date.year == today_exploded.year &&
         selected_date.month == today_exploded.month &&
         selected_date.day_of_month == today_exploded.day_of_month;
}

base::Time::Exploded GetExploded(const base::Time& date) {
  base::Time::Exploded exploded;
  date.LocalExplode(&exploded);
  return exploded;
}

}  // namespace ash
