// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/shutdown_watcher_helper.h"

#include "build/build_config.h"
#include "chrome/browser/metrics/thread_watcher_report_hang.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

// ShutdownWatcherHelper is not available on Android.
#if !BUILDFLAG(IS_ANDROID)

ShutdownWatcherHelper::ShutdownWatcherHelper() = default;

ShutdownWatcherHelper::~ShutdownWatcherHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ShutdownWatcherHelper::Arm(const base::TimeDelta& duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!shutdown_watchdog_);
  shutdown_watchdog_.emplace(duration, "Shutdown watchdog thread", true, this);
  shutdown_watchdog_->Arm();
}

// static
base::TimeDelta ShutdownWatcherHelper::GetPerChannelTimeout(
    base::TimeDelta duration) {
  base::TimeDelta actual_duration = duration;

  version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    actual_duration *= 20;
  } else if (channel == version_info::Channel::BETA) {
    actual_duration *= 10;
  } else if (channel == version_info::Channel::DEV) {
    actual_duration *= 4;
  } else {
    actual_duration *= 2;
  }

  return actual_duration;
}

void ShutdownWatcherHelper::Alarm() {
  metrics::ShutdownHang();
}

#endif  // !BUILDFLAG(IS_ANDROID)
