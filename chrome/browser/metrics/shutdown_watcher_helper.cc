// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/shutdown_watcher_helper.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/thread_watcher_report_hang.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

// TODO(crbug.com/40267942): This code is not related to the metrics
// infrastructure and should be moved to a new home.

// ShutdownWatcherHelper is not available on Android.
#if !BUILDFLAG(IS_ANDROID)

namespace {
base::TimeDelta GetPerChannelTimeout(base::TimeDelta duration) {
  base::TimeDelta actual_duration = duration;

  // TODO(crbug.com/40267942): These timeouts were set based on historical
  // values, but should be revisited. See discussion in
  // https://crrev.com/c/4527815/comments/baea15f7_98f5a0e9
  //
  // In particular, `Arm` is called with a 5 minute timeout, which translates
  // to an extremely long 100 minute shutdown timeout on stable. This is long
  // enough that we effectively are not looking for shutdown hangs on stable at
  // all.
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
}  // namespace

ShutdownWatcherHelper::ShutdownWatcherHelper() = default;

ShutdownWatcherHelper::~ShutdownWatcherHelper() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ShutdownWatcherHelper::Arm(const base::TimeDelta& duration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!shutdown_watchdog_);
  shutdown_watchdog_.emplace(GetPerChannelTimeout(duration),
                             "Shutdown watchdog thread", true, this);
  shutdown_watchdog_->Arm();
}

void ShutdownWatcherHelper::Alarm() {
  metrics::ShutdownHang();
}

#endif  // !BUILDFLAG(IS_ANDROID)
