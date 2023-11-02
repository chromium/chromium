// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_features.h"

#include "base/feature_list.h"

namespace base {

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kRemoveIOSPowerEventNotifications,
             "RemoveIOSPowerEventNotifications",
             FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace base
