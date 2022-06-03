// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_device_source.h"

namespace base {

PowerMonitorDeviceSource::PowerMonitorDeviceSource() {
#if defined(OS_APPLE)
  PlatformInit();
#endif
}

PowerMonitorDeviceSource::~PowerMonitorDeviceSource() {
#if defined(OS_APPLE)
  PlatformDestroy();
#endif
}

}  // namespace base
