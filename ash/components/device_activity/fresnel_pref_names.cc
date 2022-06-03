// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/fresnel_pref_names.h"

namespace ash {
namespace prefs {

// Timestamp of last known daily ping to Fresnel.
const char kDeviceActiveLastKnownDailyPingTimestamp[] =
    "device_active.last_known_daily_ping_timestamp";

// Timestamp of last known monthly ping to Fresnel.
const char kDeviceActiveLastKnownMonthlyPingTimestamp[] =
    "device_active.last_known_monthly_ping_timestamp";

// Timestamp of last known all time ping to Fresnel.
const char kDeviceActiveLastKnownAllTimePingTimestamp[] =
    "device_active.last_known_all_time_ping_timestamp";

}  // namespace prefs
}  // namespace ash
