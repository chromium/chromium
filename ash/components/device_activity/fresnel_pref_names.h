// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_

#include "base/component_export.h"

namespace ash {
namespace prefs {

// ---------------------------------------------------------------------------
// Prefs related to ChromeOS device active pings.
// ---------------------------------------------------------------------------

COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownDailyPingTimestamp[];
COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownMonthlyPingTimestamp[];
COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY)
extern const char kDeviceActiveLastKnownAllTimePingTimestamp[];

}  // namespace prefs
}  // namespace ash
#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_FRESNEL_PREF_NAMES_H_
