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

namespace {

// ShutdownWatchDogThread methods and members.
//
// Class for detecting hangs during shutdown.
class ShutdownWatchDogThread : public base::Watchdog {
 public:
  // Constructor specifies how long the ShutdownWatchDogThread will wait before
  // alarming.
  explicit ShutdownWatchDogThread(const base::TimeDelta& duration)
      : base::Watchdog(duration, "Shutdown watchdog thread", true) {}

  ShutdownWatchDogThread(const ShutdownWatchDogThread&) = delete;
  ShutdownWatchDogThread& operator=(const ShutdownWatchDogThread&) = delete;

  // Alarm is called if the time expires after an Arm() without someone calling
  // Disarm(). We crash the browser if this method is called.
  void Alarm() override { metrics::ShutdownHang(); }
};

}  // namespace

// ShutdownWatcherHelper methods and members.
//
// ShutdownWatcherHelper is a wrapper class for detecting hangs during
// shutdown.
ShutdownWatcherHelper::ShutdownWatcherHelper()
    : shutdown_watchdog_(nullptr),
      thread_id_(base::PlatformThread::CurrentId()) {}

ShutdownWatcherHelper::~ShutdownWatcherHelper() {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  if (shutdown_watchdog_) {
    shutdown_watchdog_->Disarm();
    delete shutdown_watchdog_;
    shutdown_watchdog_ = nullptr;
  }
}

void ShutdownWatcherHelper::Arm(const base::TimeDelta& duration) {
  DCHECK_EQ(thread_id_, base::PlatformThread::CurrentId());
  DCHECK(!shutdown_watchdog_);
  shutdown_watchdog_ =
      new ShutdownWatchDogThread(GetPerChannelTimeout(duration));
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

#endif  // !BUILDFLAG(IS_ANDROID)
