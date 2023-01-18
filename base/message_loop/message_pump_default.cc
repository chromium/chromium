// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_default.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <mach/thread_policy.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/threading/threading_features.h"
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_APPLE)
bool g_use_thread_qos = true;
#endif

}  // namespace

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(WaitableEvent::ResetPolicy::AUTOMATIC,
             WaitableEvent::InitialState::NOT_SIGNALED) {
  event_.declare_only_used_while_idle();
}

MessagePumpDefault::~MessagePumpDefault() = default;

void MessagePumpDefault::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);

  for (;;) {
#if BUILDFLAG(IS_APPLE)
    mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool has_more_immediate_work = next_work_info.is_immediate();
    if (!keep_running_)
      break;

    if (has_more_immediate_work)
      continue;

    has_more_immediate_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (has_more_immediate_work)
      continue;

    if (next_work_info.delayed_run_time.is_max()) {
      event_.Wait();
    } else {
      event_.TimedWait(next_work_info.remaining_delay());
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // Since this is always called from the same thread as Run(), there is nothing
  // to do as the loop is already running. It will wait in Run() with the
  // correct timeout when it's out of immediate tasks.
  // TODO(gab): Consider removing ScheduleDelayedWork() when all pumps function
  // this way (bit.ly/merge-message-pump-do-work).
}

#if BUILDFLAG(IS_APPLE)
void MessagePumpDefault::SetTimerSlack(TimerSlack timer_slack) {
  if (!g_use_thread_qos) {
    thread_latency_qos_policy_data_t policy{};
    policy.thread_latency_qos_tier = timer_slack == TIMER_SLACK_MAXIMUM
                                         ? LATENCY_QOS_TIER_3
                                         : LATENCY_QOS_TIER_UNSPECIFIED;
    mac::ScopedMachSendRight thread_port(mach_thread_self());
    kern_return_t kr =
        thread_policy_set(thread_port.get(), THREAD_LATENCY_QOS_POLICY,
                          reinterpret_cast<thread_policy_t>(&policy),
                          THREAD_LATENCY_QOS_POLICY_COUNT);
    MACH_DVLOG_IF(1, kr != KERN_SUCCESS, kr) << "thread_policy_set";
  }
}

// static
void MessagePumpDefault::InitFeaturesPostFieldTrial() {
  // Since kUseThreadQoSMac is not constexpr (forbidden for Features), it cannot
  // be used to initialize |g_use_thread_qos| at compile time. At least DCHECK
  // that its initial value matches the default value of the feature here.
  DCHECK_EQ(g_use_thread_qos,
            kUseThreadQoSMac.default_state == FEATURE_ENABLED_BY_DEFAULT);

  // A DCHECK is triggered on FeatureList initialization if the state of a
  // feature has been checked before. To avoid triggering this DCHECK in unit
  // tests that call this before initializing the FeatureList, only check the
  // state of the feature if the FeatureList is initialized.
  if (FeatureList::GetInstance()) {
    g_use_thread_qos = FeatureList::IsEnabled(kUseThreadQoSMac);
  }
}
#endif

}  // namespace base
