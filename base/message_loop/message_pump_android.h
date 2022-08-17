// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_ANDROID_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// ALooper 除了提供message机制之外，还提供了监听文件描述符的方式。
// 通过addFd()接口加入需要被监听的文件描述符。
struct ALooper; // Android消息循环的核心

namespace base {

class RunLoop;

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_ANDROID platform.
// 此类实现了 OS_ANDROID 平台上 TYPE_UI MessageLoops 所需的 MessagePump
// 处理Android平台消息泵的UI事件，核心是利用ALoop来驱动消息泵的运行
class BASE_EXPORT MessagePumpForUI : public MessagePump {
 public:
  MessagePumpForUI();

  MessagePumpForUI(const MessagePumpForUI&) = delete;
  MessagePumpForUI& operator=(const MessagePumpForUI&) = delete;

  ~MessagePumpForUI() override;

  // 消息泵的基本接口函数，用于外部使用方传入真正任务执行队列
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

  // Attaches |delegate| to this native MessagePump. |delegate| will from
  // then on be invoked by the native loop to process application tasks.
  // 附上|代理| 到这个本机 MessagePump。 |代理| 从此时起将由本机循环调用以处理应
  // 用程序任务。
  virtual void Attach(Delegate* delegate);

  // We call Abort when there is a pending JNI exception, meaning that the
  // current thread will crash when we return to Java.
  // We can't call any JNI-methods before returning to Java as we would then
  // cause a native crash (instead of the original Java crash).
  void Abort() { should_abort_ = true; }
  bool IsAborted() { return should_abort_; }
  bool ShouldQuit() const { return should_abort_ || quit_; }

  // Tells the RunLoop to quit when idle, calling the callback when
  // it's safe for the Thread to stop.
  // 告诉 RunLoop 在空闲时退出，在线程安全停止时调用回调。
  void QuitWhenIdle(base::OnceClosure callback);

  // These functions are only public so that the looper callbacks can
  // call them, and should not be called from outside this class.
  void OnDelayedLooperCallback();
  void OnNonDelayedLooperCallback();

 protected:
  Delegate* SetDelegate(Delegate* delegate);
  bool SetQuit(bool quit);
  virtual void DoDelayedLooperWork();
  virtual void DoNonDelayedLooperWork(bool do_idle_work);

 private:
  void ScheduleWorkInternal(bool do_idle_work);
  void DoIdleWork();

  // Unlike other platforms, we don't control the message loop as it's
  // controlled by the Android Looper, so we can't run a RunLoop to keep the
  // Thread this pump belongs to alive. However, threads are expected to have an
  // active run loop, so we manage a RunLoop internally here, starting/stopping
  // it as necessary.
  // 与其他平台不同，我们不控制消息循环，因为它由 Android Looper 控制，因此我们无法
  // 运行 RunLoop 来保持该泵所属的线程处于活动状态。但是，线程应该有一个活动的运行循
  // 环，所以我们在这里内部管理一个运行循环，根据需要启动/停止它。
  std::unique_ptr<RunLoop> run_loop_;

  // See Abort().
  bool should_abort_ = false;

  // Whether this message pump is quitting, or has quit.
  bool quit_ = false;

  // The MessageLoop::Delegate for this pump.
  Delegate* delegate_ = nullptr;

  // The time at which we are currently scheduled to wake up and perform a
  // delayed task. This avoids redundantly scheduling |delayed_fd_| with the
  // same timeout when subsequent work phases all go idle on the same pending
  // delayed task; nullopt if no wakeup is currently scheduled.
  absl::optional<TimeTicks> delayed_scheduled_time_;

  // If set, a callback to fire when the message pump is quit.
  base::OnceClosure on_quit_callback_;

  // The file descriptor used to signal that non-delayed work is available.
  int non_delayed_fd_;

  // The file descriptor used to signal that delayed work is available.
  int delayed_fd_;

  // The Android Looper for this thread.
  ALooper* looper_ = nullptr; // 核心，消息泵的引擎

  // The JNIEnv* for this thread, used to check for pending exceptions.
  JNIEnv* env_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_ANDROID_H_
