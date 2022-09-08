// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor_device_source.h"

#include "build/build_config.h"

namespace base {

PowerMonitorDeviceSource::PowerMonitorDeviceSource() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  PlatformInit();
#endif
}

PowerMonitorDeviceSource::~PowerMonitorDeviceSource() {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  PlatformDestroy();
#endif
}

}  // namespace base
