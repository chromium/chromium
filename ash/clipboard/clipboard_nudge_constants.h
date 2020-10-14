// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_

#include "base/time/time.h"

namespace ash {

constexpr static int kNotificationLimit = 3;
constexpr static base::TimeDelta kMinInterval = base::TimeDelta::FromDays(1);
constexpr static base::TimeDelta kMaxTimeBetweenPaste =
    base::TimeDelta::FromMinutes(10);
constexpr static base::TimeDelta kNudgeShowTime =
    base::TimeDelta::FromSeconds(6);

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONSTANTS_H_
