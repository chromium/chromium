// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_DEFAULT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_DEFAULT_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

/**
 * @brief 简单消息泵
 * MessagePump 在初始化时（Init函数）会注册事件，然后在 Run 函数循环过程中会等待该事件。
 * 其中 MessagePumpDefault 最简单，它只能处理Task任务，所以它 Run 时直接阻塞在一个条件
 * 变量上即可，然后等待 ScheduleWork 函数来唤醒它。ScheduleWork什么时候调用呢？已经很明确了，
 * 在有人调用MessageLoop::AddToIncomingQueue向该线程的消息incoming队列中放任务时，就
 * 要调用MessagePumpDefault的ScheduleWork函数。
 */

class BASE_EXPORT MessagePumpDefault : public MessagePump {
 public:
  MessagePumpDefault();

  MessagePumpDefault(const MessagePumpDefault&) = delete;
  MessagePumpDefault& operator=(const MessagePumpDefault&) = delete;

  ~MessagePumpDefault() override;

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  /**
   * @brief 唤醒（调度）任务
   */
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;
#if defined(OS_APPLE)
  // 设置定时器松弛
  void SetTimerSlack(TimerSlack timer_slack) override;
#endif

 private:
  // This flag is set to false when Run should return.
  // 当 Run 应该返回时，此标志设置为 false。
  bool keep_running_;

  // Used to sleep until there is more work to do.
  // 用于休眠，直到有更多的工作要做，类似：std::condition_variable
  WaitableEvent event_; // std::condition_variable
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_DEFAULT_H_
