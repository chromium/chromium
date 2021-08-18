// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_UTILS_H_
#define ASH_SYSTEM_TIME_CALENDAR_UTILS_H_

#include "base/time/time.h"

namespace ash {

// Checks if the `selected_date` is local time today.
bool IsToday(const base::Time::Exploded& selected_date);

// Gets the given `date`'s `Exploded` instance.
base::Time::Exploded GetExploded(const base::Time& date);

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_UTILS_H_