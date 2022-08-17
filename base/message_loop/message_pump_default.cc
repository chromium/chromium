// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 一个默认的事件循环执行的代码，但是Mac的Chrome的渲染线程并不是执行的那里的，它的
 * 事件循环使用了Mac Cocoa sdk的NSRunLoop，根据源码的解释，是因为页面的滚动条、
 * select下拉弹框是用的Cocoa的，所以必须接入Cococa的事件循环机制。
 *
 * 如果是OS_MACOSX的话，消息循环泵pump就是用的NSRunLoop的，，否则的话就用默认的.
 * 这个泵pump的意思应该就是指消息的源头。
 *
 * Cococa的pump和默认的pump都有统一对外的接口。例如都有一个ScheduleWork函数
 * 去唤醒线程，只是里面的实现不一样，如唤醒的方式不一样。
 *
 * Chrome IO线程（包括页面进程的子IO线程）在默认的pump上面又加了一个libevent
 * 库提供的消息循环。libevent是一个跨平台的事件驱动的网络库，主要是拿来做socket
 * 编程的，以事件驱动的方式。接入libevent的pump文件叫message_pump_libevent.cc.
 */

#include "base/message_loop/message_pump_default.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(OS_APPLE)
#include <mach/thread_policy.h>

#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace base {

MessagePumpDefault::MessagePumpDefault()
    : keep_running_(true),
      event_(WaitableEvent::ResetPolicy::AUTOMATIC,
             WaitableEvent::InitialState::NOT_SIGNALED) {
  event_.declare_only_used_while_idle();
}

MessagePumpDefault::~MessagePumpDefault() = default;

/**
 * @brief 首先代码在一个for死循环里面执行，第一步先调用DoWork遍历并取出任务队列里
 * 所有非delayed的pending_task执行，部分任务可能会被deferred到后面第三步
 * DoIdlWork再执行，第二步是执行那些delayed的任务，如果当前不能立刻执行，那么设置
 * 一个等待的时间delayed_work_time_，并且返回did_work是false，执行到最后面代码
 * 的TimedWaitUntil等待时间后唤醒执行。
 * 这就是多线程事件循环的基本模型。那么多线程要执行的task是从哪里来的呢？
 *
 * @param delegate
 */
void MessagePumpDefault::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);

  for (;;) {
#if defined(OS_APPLE) // MAC独有的，其他平台为空实现
    mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    // DoWork会去执行当前所有的pending_task（在队列里面）
    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool has_more_immediate_work = next_work_info.is_immediate();
    if (!keep_running_)
      break;

    if (has_more_immediate_work)
      continue;

    // idle任务是在第一步没有执行被deferred的任务
    has_more_immediate_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (has_more_immediate_work)
      continue;

    if (next_work_info.delayed_run_time.is_max()) {
      // 没有delay时间就一直睡着，直到有人PostTask过来
      event_.Wait();
    } else {
      // 如果有delay的时间，那么进行睡眠直到时间到被唤醒
      event_.TimedWait(next_work_info.remaining_delay());
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }
}

void MessagePumpDefault::Quit() {
  keep_running_ = false;
}

/**
 * @brief 启动计划工作
 */
void MessagePumpDefault::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  // 由于这可以在任何线程上调用，我们需要确保我们的 Run 循环被唤醒。
  event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  // Since this is always called from the same thread as Run(), there is nothing
  // to do as the loop is already running. It will wait in Run() with the
  // correct timeout when it's out of immediate tasks.
  // TODO(gab): Consider removing ScheduleDelayedWork() when all pumps function
  // this way (bit.ly/merge-message-pump-do-work).
}

#if defined(OS_APPLE)
void MessagePumpDefault::SetTimerSlack(TimerSlack timer_slack) {
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
#endif

}  // namespace base
