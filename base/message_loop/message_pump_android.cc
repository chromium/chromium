// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_android.h"

// Android消息循环核心 ALoop，提供消息泵UI事件处理引擎
#include <android/looper.h>

#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "build/build_config.h"

// Android stripped sys/timerfd.h out of their platform headers, so we have to
// use syscall to make use of timerfd. Once the min API level is 20, we can
// directly use timerfd.h.
#ifndef __NR_timerfd_create
#error "Unable to find syscall for __NR_timerfd_create"
#endif

#ifndef TFD_TIMER_ABSTIME
#define TFD_TIMER_ABSTIME (1 << 0)
#endif

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace base {

namespace {

// See sys/timerfd.h
int timerfd_create(int clockid, int flags) {
  return syscall(__NR_timerfd_create, clockid, flags);
}

// See sys/timerfd.h
int timerfd_settime(int ufc,
                    int flags,
                    const struct itimerspec* utmr,
                    struct itimerspec* otmr) {
  return syscall(__NR_timerfd_settime, ufc, flags, utmr, otmr);
}

// https://crbug.com/873588. The stack may not be aligned when the ALooper calls
// into our code due to the inconsistent ABI on older Android OS versions.
// 由于旧 Android OS 版本上的 ABI 不一致，当 ALooper 调用我们的代码时，堆栈可能
// 不会对齐。
#if defined(ARCH_CPU_X86)
#define STACK_ALIGN __attribute__((force_align_arg_pointer))
#else
#define STACK_ALIGN
#endif

// 没有延时的回调函数，对应的事件是：ALOOPER_EVENT_INPUT，在ALoop消息循环中
// 被唤醒执行
STACK_ALIGN int NonDelayedLooperCallback(int fd, int events, void* data) {
  if (events & ALOOPER_EVENT_HANGUP) // 挂起事件
    return 0;

  DCHECK(events & ALOOPER_EVENT_INPUT); // 读事件
  MessagePumpForUI* pump = reinterpret_cast<MessagePumpForUI*>(data);
  pump->OnNonDelayedLooperCallback(); // 调用没有延时的回调函数
  return 1;  // continue listening for events
}

// 有延时的定时器回调函数，对应的事件是：ALOOPER_EVENT_INPUT，
// 在ALoop消息循环中被唤醒执行
STACK_ALIGN int DelayedLooperCallback(int fd, int events, void* data) {
  if (events & ALOOPER_EVENT_HANGUP)
    return 0;

  DCHECK(events & ALOOPER_EVENT_INPUT);
  MessagePumpForUI* pump = reinterpret_cast<MessagePumpForUI*>(data);
  pump->OnDelayedLooperCallback();
  return 1;  // continue listening for events
}

// A bit added to the |non_delayed_fd_| to keep it signaled when we yield to
// native tasks below.
// 在 |non_delayed_fd_| 中添加了一点 当我们屈服于下面的本机任务时，保持它的信号。
constexpr uint64_t kTryNativeTasksBeforeIdleBit = uint64_t(1) << 32;
}  // namespace

/**
 * @brief 使用 Android系统的ALoop(pipe+epoll)来监听定时器事件fd和非定时器事件fd,
 * 并设置回调函数.
 */
MessagePumpForUI::MessagePumpForUI()
    : env_(base::android::AttachCurrentThread()) {

  // The Android native ALooper uses epoll to poll our file descriptors and wake
  // us up. We use a simple level-triggered eventfd to signal that non-delayed
  // work is available, and a timerfd to signal when delayed work is ready to
  // be run.
  // Android 原生 ALooper，本质是 fifo + epoll，使用 epoll 来轮询我们的文件描述符(fd)
  // 并唤醒我们。我们使用一个简单的电平触发 eventfd 来表示非延迟工作可用，并使用 timerfd
  // 来表示延迟工作何时可以运行。
  // eventfd()其实是内核为应用程序提供的信号量。它相比于POSIX信号量的优势是，在内核
  // 里以文件形式存在，可用于select/epoll循环中，因此可以实现异步的信号量，避免了消
  // 费者在资源不可用时的阻塞。这也是为什么取名叫eventfd的原因：event表示它可用来作
  // 事件通知（当然是异步的），fd表示它是一个“文件”。
  non_delayed_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  CHECK_NE(non_delayed_fd_, -1);
  DCHECK_EQ(TimeTicks::GetClock(), TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);

  // We can't create the timerfd with TFD_NONBLOCK | TFD_CLOEXEC as we can't
  // include timerfd.h. See comments above on __NR_timerfd_create. It looks like
  // they're just aliases to O_NONBLOCK and O_CLOEXEC anyways, so this should be
  // fine.
  // 我们无法使用 TFD_NONBLOCK | 创建 timerfd | TFD_CLOEXEC，因为我们不能包含 timerfd.h。
  // 请参阅上面关于 __NR_timerfd_create 的评论。 看起来它们只是 O_NONBLOCK 和 O_CLOEXEC
  // 的别名，所以这应该没问题。
  // 创建定时器事件fd
  delayed_fd_ = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK | O_CLOEXEC);
  CHECK_NE(delayed_fd_, -1);

  // 准备一个与调用线程关联的循环器，并返回它
  looper_ = ALooper_prepare(0); // Android消息循环初始化
  DCHECK(looper_);
  // Add a reference to the looper so it isn't deleted on us.
  // 添加对 looper 的引用，这样当我们使用时它就不会被删除.
  ALooper_acquire(looper_);
  // 添加由 ALoop 轮询的fd：未延时和延时fd，并设置时间过滤器和回调函数
  ALooper_addFd(looper_, non_delayed_fd_, // 监听的文件描述符
                0,
                ALOOPER_EVENT_INPUT, // 读事件
                &NonDelayedLooperCallback, // 回调函数
                reinterpret_cast<void*>(this)); // 把this传递给回调函数中使用
  ALooper_addFd(looper_, delayed_fd_, 0, ALOOPER_EVENT_INPUT,
                &DelayedLooperCallback, reinterpret_cast<void*>(this));
}

MessagePumpForUI::~MessagePumpForUI() {
  // ALooper_forThread()()返回与调用线程关联的 ALoop实例，如果没有则返回 NULL
  DCHECK_EQ(ALooper_forThread(), looper_);
  // 从 looper 中删除先前添加的文件描述符
  ALooper_removeFd(looper_, non_delayed_fd_);
  ALooper_removeFd(looper_, delayed_fd_);
  // 删除以前使用ALooper_acquire()获取的引用
  ALooper_release(looper_);
  looper_ = nullptr;

  close(non_delayed_fd_);
  close(delayed_fd_);
}

/**
 * @brief 间接在定时器时间fd触发时调用的回调函数
 */
void MessagePumpForUI::OnDelayedLooperCallback() {
  // There may be non-Chromium callbacks on the same ALooper which may have left
  // a pending exception set, and ALooper does not check for this between
  // callbacks. Check here, and if there's already an exception, just skip this
  // iteration without clearing the fd. If the exception ends up being non-fatal
  // then we'll just get called again on the next polling iteration.
  if (base::android::HasException(env_))
    return;

  // ALooper_pollOnce may call this after Quit() if OnNonDelayedLooperCallback()
  // resulted in Quit() in the same round.
  if (ShouldQuit())
    return;

  // Clear the fd.
  uint64_t value;
  // 从定时器事件fd中从读取
  int ret = read(delayed_fd_, &value, sizeof(value));

  // TODO(mthiesse): Figure out how it's possible to hit EAGAIN here.
  // According to http://man7.org/linux/man-pages/man2/timerfd_create.2.html
  // EAGAIN only happens if no timer has expired. Also according to the man page
  // poll only returns readable when a timer has expired. So this function will
  // only be called when a timer has expired, but reading reveals no timer has
  // expired...
  // Quit() and ScheduleDelayedWork() are the only other functions that touch
  // the timerfd, and they both run on the same thread as this callback, so
  // there are no obvious timing or multi-threading related issues.
  DPCHECK(ret >= 0 || errno == EAGAIN);
  DoDelayedLooperWork(); // 执行delete->DoWork()，即执行外部消息队列
}

void MessagePumpForUI::DoDelayedLooperWork() {
  delayed_scheduled_time_.reset();

  Delegate::NextWorkInfo next_work_info = delegate_->DoWork();

  if (ShouldQuit())
    return;

  if (next_work_info.is_immediate()) {
    ScheduleWork();
    return;
  }

  DoIdleWork();
  if (!next_work_info.delayed_run_time.is_max())
    ScheduleDelayedWork(next_work_info.delayed_run_time);
}

/**
 * @brief 间接在事件fd中触发时调用的回调函数
 */
void MessagePumpForUI::OnNonDelayedLooperCallback() {
  // There may be non-Chromium callbacks on the same ALooper which may have left
  // a pending exception set, and ALooper does not check for this between
  // callbacks. Check here, and if there's already an exception, just skip this
  // iteration without clearing the fd. If the exception ends up being non-fatal
  // then we'll just get called again on the next polling iteration.
  if (base::android::HasException(env_))
    return;

  // ALooper_pollOnce may call this after Quit() if OnDelayedLooperCallback()
  // resulted in Quit() in the same round.
  if (ShouldQuit())
    return;

  // We're about to process all the work requested by ScheduleWork().
  // MessagePump users are expected to do their best not to invoke
  // ScheduleWork() again before DoWork() returns a non-immediate
  // NextWorkInfo below. Hence, capturing the file descriptor's value now and
  // resetting its contents to 0 should be okay. The value currently stored
  // should be greater than 0 since work having been scheduled is the reason
  // we're here. See http://man7.org/linux/man-pages/man2/eventfd.2.html
  uint64_t value = 0;
  int ret = read(non_delayed_fd_, &value, sizeof(value));
  DPCHECK(ret >= 0);
  DCHECK_GT(value, 0U);
  // 判断是否是idel事件类型
  bool do_idle_work = value == kTryNativeTasksBeforeIdleBit;
  // 根据 do_idle_work 而执行 delegate_->DoWork() 和 delegate_->DoIdleWork()
  // 这些函数上面说过是对外的消息队的接口
  DoNonDelayedLooperWork(do_idle_work);
}

void MessagePumpForUI::DoNonDelayedLooperWork(bool do_idle_work) {
  // Note: We can't skip DoWork() even if |do_idle_work| is true here (i.e. no
  // additional ScheduleWork() since yielding to native) as delayed tasks might
  // have come in and we need to re-sample |next_work_info|.

  // Runs all application tasks scheduled to run.
  Delegate::NextWorkInfo next_work_info;
  do {
    if (ShouldQuit())
      return;

    next_work_info = delegate_->DoWork();
    // If we are prioritizing native, and the next work would normally run
    // immediately, skip the next work and let the native tasks have a chance to
    // run. This is useful when user input is waiting for native to have a
    // chance to run.
    if (next_work_info.is_immediate() && next_work_info.yield_to_native) {
      ScheduleWork();
      return;
    }
  } while (next_work_info.is_immediate());

  // Do not resignal |non_delayed_fd_| if we're quitting (this pump doesn't
  // allow nesting so needing to resume in an outer loop is not an issue
  // either).
  if (ShouldQuit())
    return;

  // Before declaring this loop idle, yield to native tasks and arrange to be
  // called again (unless we're already in that second call).
  if (!do_idle_work) {
    ScheduleWorkInternal(/*do_idle_work=*/true);
    return;
  }

  // We yielded to native tasks already and they didn't generate a
  // ScheduleWork() request so we can declare idleness. It's possible for a
  // ScheduleWork() request to come in racily while this method unwinds, this is
  // fine and will merely result in it being re-invoked shortly after it
  // returns.
  // TODO(scheduler-dev): this doesn't account for tasks that don't ever call
  // SchedulerWork() but still keep the system non-idle (e.g., the Java Handler
  // API). It would be better to add an API to query the presence of native
  // tasks instead of relying on yielding once + kTryNativeTasksBeforeIdleBit.
  DCHECK(do_idle_work);

  if (ShouldQuit())
    return;

  // At this point, the java looper might not be idle - it's impossible to know
  // pre-Android-M, so we may end up doing Idle work while java tasks are still
  // queued up. Note that this won't cause us to fail to run java tasks using
  // QuitWhenIdle, as the JavaHandlerThread will finish running all currently
  // scheduled tasks before it quits. Also note that we can't just add an idle
  // callback to the java looper, as that will fire even if application tasks
  // are still queued up.
  DoIdleWork();
  if (!next_work_info.delayed_run_time.is_max())
    ScheduleDelayedWork(next_work_info.delayed_run_time);
}

void MessagePumpForUI::DoIdleWork() {
  if (delegate_->DoIdleWork()) {
    // If DoIdleWork() resulted in any work, we're not idle yet. We need to pump
    // the loop here because we may in fact be idle after doing idle work
    // without any new tasks being queued.
    ScheduleWork();
  }
}

void MessagePumpForUI::Run(Delegate* delegate) {
  CHECK(false) << "Unexpected call to Run()";
}

void MessagePumpForUI::Attach(Delegate* delegate) {
  DCHECK(!quit_);

  // Since the Looper is controlled by the UI thread or JavaHandlerThread, we
  // can't use Run() like we do on other platforms or we would prevent Java
  // tasks from running. Instead we create and initialize a run loop here, then
  // return control back to the Looper.

  SetDelegate(delegate);
  run_loop_ = std::make_unique<RunLoop>();
  // Since the RunLoop was just created above, BeforeRun should be guaranteed to
  // return true (it only returns false if the RunLoop has been Quit already).
  if (!run_loop_->BeforeRun())
    NOTREACHED();
}

void MessagePumpForUI::Quit() {
  if (quit_)
    return;

  quit_ = true;

  int64_t value;
  // Clear any pending timer.
  read(delayed_fd_, &value, sizeof(value));
  // Clear the eventfd.
  read(non_delayed_fd_, &value, sizeof(value));

  if (run_loop_) {
    run_loop_->AfterRun();
    run_loop_ = nullptr;
  }
  if (on_quit_callback_) {
    std::move(on_quit_callback_).Run();
  }
}

/**
 * @brief 唤醒ALoop，进而驱动消息泵
 */
void MessagePumpForUI::ScheduleWork() {
  ScheduleWorkInternal(/*do_idle_work=*/false);
}

void MessagePumpForUI::ScheduleWorkInternal(bool do_idle_work) {
  // Write (add) |value| to the eventfd. This tells the Looper to wake up and
  // call our callback, allowing us to run tasks. This also allows us to detect,
  // when we clear the fd, whether additional work was scheduled after we
  // finished performing work, but before we cleared the fd, as we'll read back
  // >=2 instead of 1 in that case. See the eventfd man pages
  // (http://man7.org/linux/man-pages/man2/eventfd.2.html) for details on how
  // the read and write APIs for this file descriptor work, specifically without
  // EFD_SEMAPHORE.
  // Note: Calls with |do_idle_work| set to true may race with potential calls
  // where the parameter is false. This is fine as write() is adding |value|,
  // not overwriting the existing value, and as such racing calls would merely
  // have their values added together. Since idle work is only executed when the
  // value read equals kTryNativeTasksBeforeIdleBit, a race would prevent idle
  // work from being run and trigger another call to this method with
  // |do_idle_work| set to true.
  uint64_t value = do_idle_work ? kTryNativeTasksBeforeIdleBit : 1;
  // 向 ALoop(pipe+epoll)监听的 non_delayed_fd_ 写入数据，唤醒ALoop的回调函数执行：
  // NonDelayedLooperCallback()
  int ret = write(non_delayed_fd_, &value, sizeof(value));
  DPCHECK(ret >= 0);
}

/**
 * @brief 设置定时器事件fd：delayed_fd_，此时ALoop在监听这个 delayed_fd_，定时器到期
 * 则会触发callback的执行：DelayedLooperCallback()，从而触发deleagte设定的定时器回调
 * 函数：
 */
void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  if (ShouldQuit())
    return;

  if (delayed_scheduled_time_ && *delayed_scheduled_time_ == delayed_work_time)
    return;

  DCHECK(!delayed_work_time.is_null());
  delayed_scheduled_time_ = delayed_work_time;
  int64_t nanos = delayed_work_time.since_origin().InNanoseconds();
  struct itimerspec ts;
  ts.it_interval.tv_sec = 0;  // Don't repeat.
  ts.it_interval.tv_nsec = 0;
  ts.it_value.tv_sec = nanos / TimeTicks::kNanosecondsPerSecond;
  ts.it_value.tv_nsec = nanos % TimeTicks::kNanosecondsPerSecond;
  // 给delayed_fd（延时文件描述符）设置延时的绝对时间，等时间到了后，delayed_fd_会
  // 有可读事件，这样会在回调函数中处理了
  // syscall(__NR_timerfd_settime, ufc, flags, utmr, otmr);
  int ret = timerfd_settime(delayed_fd_, TFD_TIMER_ABSTIME, &ts, nullptr);
  DPCHECK(ret >= 0);
}

void MessagePumpForUI::QuitWhenIdle(base::OnceClosure callback) {
  DCHECK(!on_quit_callback_);
  DCHECK(run_loop_);
  on_quit_callback_ = std::move(callback);
  run_loop_->QuitWhenIdle();
  // Pump the loop in case we're already idle.
  ScheduleWork();
}

MessagePump::Delegate* MessagePumpForUI::SetDelegate(Delegate* delegate) {
  return std::exchange(delegate_, delegate);
}

bool MessagePumpForUI::SetQuit(bool quit) {
  return std::exchange(quit_, quit);
}

}  // namespace base
