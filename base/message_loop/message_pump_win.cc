// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_win.h"

#include <winbase.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <type_traits>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_features.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_message_pump.pbzero.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

namespace {

// Returns the number of milliseconds before |next_task_time|, clamped between
// zero and the biggest DWORD value (or INFINITE if |next_task_time.is_max()|).
// Optionally, a recent value of Now() may be passed in to avoid resampling it.
DWORD GetSleepTimeoutMs(TimeTicks next_task_time,
                        TimeTicks recent_now = TimeTicks()) {
  // Shouldn't need to sleep or install a timer when there's pending immediate
  // work.
  DCHECK(!next_task_time.is_null());

  if (next_task_time.is_max())
    return INFINITE;

  auto now = recent_now.is_null() ? TimeTicks::Now() : recent_now;
  auto timeout_ms = (next_task_time - now).InMillisecondsRoundedUp();

  // A saturated_cast with an unsigned destination automatically clamps negative
  // values at zero.
  static_assert(!std::is_signed_v<DWORD>, "DWORD is unexpectedly signed");
  return saturated_cast<DWORD>(timeout_ms);
}

bool g_ui_pump_improvements_win = false;

}  // namespace

// Message sent to get an additional time slice for pumping (processing) another
// task (a series of such messages creates a continuous task pump).
static const int kMsgHaveWork = WM_USER + 1;

//-----------------------------------------------------------------------------
// MessagePumpWin public:

MessagePumpWin::MessagePumpWin() = default;
MessagePumpWin::~MessagePumpWin() = default;

// static
void MessagePumpWin::InitializeFeatures() {
  g_ui_pump_improvements_win = FeatureList::IsEnabled(kUIPumpImprovementsWin);
}

void MessagePumpWin::Run(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  RunState run_state(delegate);
  if (run_state_)
    run_state.is_nested = true;

  AutoReset<raw_ptr<RunState>> auto_reset_run_state(&run_state_, &run_state);
  DoRunLoop();
}

void MessagePumpWin::Quit() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  DCHECK(run_state_);
  run_state_->should_quit = true;
}

//-----------------------------------------------------------------------------
// MessagePumpForUI public:

MessagePumpForUI::MessagePumpForUI() {
  bool succeeded = message_window_.Create(
      BindRepeating(&MessagePumpForUI::MessageCallback, Unretained(this)));
  PCHECK(succeeded) << "Failed to create message-only Window";
}

MessagePumpForUI::~MessagePumpForUI() = default;

void MessagePumpForUI::ScheduleWork() {
  // This is the only MessagePumpForUI method which can be called outside of
  // |bound_thread_|.

  if (g_ui_pump_improvements_win &&
      !in_nested_native_loop_with_application_tasks_) {
    // The pump is running using `event_` as its chrome-side synchronization
    // variable. In this case, no deduplication is done, since the event has its
    // own state.
    event_.Signal();
    return;
  }

  bool not_scheduled = false;
  if (!native_msg_scheduled_.compare_exchange_strong(
          not_scheduled, true, std::memory_order_relaxed)) {
    return;  // Someone else continued the pumping.
  }

  const BOOL ret = ::PostMessage(message_window_.hwnd(), kMsgHaveWork, 0, 0);
  if (ret) {
    return;  // There was room in the Window Message queue.
  }

  // We have failed to insert a have-work message, so there is a chance that we
  // will starve tasks/timers while sitting in a nested run loop. Nested loops
  // only look at Windows Message queues, and don't look at *our* task queues,
  // etc., so we might not get a time slice in such. :-(
  // We could abort here, but the fear is that this failure mode is plausibly
  // common (queue is full, of about 2000 messages), so we'll do a near-graceful
  // recovery.  Nested loops are pretty transient (we think), so this will
  // probably be recoverable.

  // Clarify that we didn't really insert.
  native_msg_scheduled_.store(false, std::memory_order_relaxed);
  TRACE_EVENT_INSTANT0("base", "Chrome.MessageLoopProblem.MESSAGE_POST_ERROR",
                       TRACE_EVENT_SCOPE_THREAD);
}

void MessagePumpForUI::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // Since this is always called from |bound_thread_|, there is almost always
  // nothing to do as the loop is already running. When the loop becomes idle,
  // it will typically WaitForWork() in DoRunLoop() with the timeout provided by
  // DoWork(). The only alternative to this is entering a native nested loop
  // (e.g. modal dialog) under a
  // `ScopedAllowApplicationTasksInNativeNestedLoop`, in which case
  // HandleWorkMessage() will be invoked when the system picks up kMsgHaveWork
  // and it will ScheduleNativeTimer() if it's out of immediate work. However,
  // in that alternate scenario : it's possible for a Windows native work item
  // (e.g. https://docs.microsoft.com/en-us/windows/desktop/winmsg/using-hooks)
  // to wake the native nested loop and PostDelayedTask() to the current thread
  // from it. This is the only case where we must install/adjust the native
  // timer from ScheduleDelayedWork() because if we don't, the native loop will
  // go back to sleep, unaware of the new |delayed_work_time|.
  // See MessageLoopTest.PostDelayedTaskFromSystemPump for an example.
  // TODO(gab): This could potentially be replaced by a ForegroundIdleProc hook
  // if Windows ends up being the only platform requiring ScheduleDelayedWork().
  if (in_nested_native_loop_with_application_tasks_ &&
      !native_msg_scheduled_.load(std::memory_order_relaxed)) {
    ScheduleNativeTimer(next_work_info);
  }
}

bool MessagePumpForUI::HandleNestedNativeLoopWithApplicationTasks(
    bool application_tasks_desired) {
  // It is here assumed that we will be in a native loop until either
  // DoRunLoop() gets control back, or this is called with `false`, and thus the
  // Windows event queue is to be used for synchronization. This is to prevent
  // being unable to wake up for application tasks in the case of a nested loop.
  in_nested_native_loop_with_application_tasks_ = application_tasks_desired;
  if (application_tasks_desired) {
    ScheduleWork();
  }
  return true;
}

void MessagePumpForUI::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  observers_.AddObserver(observer);
}

void MessagePumpForUI::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  observers_.RemoveObserver(observer);
}

//-----------------------------------------------------------------------------
// MessagePumpForUI private:

bool MessagePumpForUI::MessageCallback(
    UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  switch (message) {
    case kMsgHaveWork:
      HandleWorkMessage();
      break;
    case WM_TIMER:
      if (wparam == reinterpret_cast<UINT_PTR>(this))
        HandleTimerMessage();
      break;
  }
  return false;
}

void MessagePumpForUI::DoRunLoop() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // IF this was just a simple PeekMessage() loop (servicing all possible work
  // queues), then Windows would try to achieve the following order according
  // to MSDN documentation about PeekMessage with no filter):
  //    * Sent messages
  //    * Posted messages
  //    * Sent messages (again)
  //    * WM_PAINT messages
  //    * WM_TIMER messages
  //
  // Summary: none of the above classes is starved, and sent messages has twice
  // the chance of being processed (i.e., reduced service time).

  wakeup_state_ = WakeupState::kRunning;

  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // ProcessNextWindowsMessage(), there is a good chance there are still more
    // messages waiting.  On the other hand, when any of these methods return
    // having done no work, then it is pretty unlikely that calling them again
    // quickly will find any work to do. Finally, if they all say they had no
    // work, then it is a good time to consider sleeping (waiting) for more
    // work.

    in_nested_native_loop_with_application_tasks_ = false;
    bool more_work_is_plausible = false;

    if (!g_ui_pump_improvements_win ||
        wakeup_state_ != WakeupState::kApplicationTask) {
      more_work_is_plausible |= ProcessNextWindowsMessage();
      // We can end up in native loops which allow application tasks outside of
      // DoWork() when Windows calls back a Win32 message window owned by some
      // Chromium code.
      in_nested_native_loop_with_application_tasks_ = false;
      if (run_state_->should_quit) {
        break;
      }
    }

    Delegate::NextWorkInfo next_work_info = run_state_->delegate->DoWork();
    // Since nested native loops with application tasks are initiated by a
    // scoper, they should always be cleared before exiting DoWork().
    DCHECK(!in_nested_native_loop_with_application_tasks_);
    wakeup_state_ = WakeupState::kRunning;
    more_work_is_plausible |= next_work_info.is_immediate();

    if (run_state_->should_quit) {
      break;
    }

    if (installed_native_timer_) {
      // As described in ScheduleNativeTimer(), the native timer is only
      // installed and needed while in a nested native loop. If it is installed,
      // it means the above work entered such a loop. Having now resumed, the
      // native timer is no longer needed.
      KillNativeTimer();
    }

    if (more_work_is_plausible)
      continue;

    run_state_->delegate->DoIdleWork();
    // DoIdleWork() shouldn't end up in native nested loops, nor should it
    // permit native nested loops, and thus shouldn't have any chance of
    // reinstalling a native timer.
    DCHECK(!in_nested_native_loop_with_application_tasks_);
    DCHECK(!installed_native_timer_);
    if (run_state_->should_quit) {
      break;
    }

    WaitForWork(next_work_info);
  }
}

void MessagePumpForUI::WaitForWork(Delegate::NextWorkInfo next_work_info) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // Wait until a message is available, up to the time needed by the timer
  // manager to fire the next set of timers.
  DWORD wait_flags = MWMO_INPUTAVAILABLE;
  bool last_wakeup_was_spurious = false;
  for (DWORD delay = GetSleepTimeoutMs(next_work_info.delayed_run_time,
                                       next_work_info.recent_now);
       delay != 0; delay = GetSleepTimeoutMs(next_work_info.delayed_run_time)) {
    if (!last_wakeup_was_spurious) {
      run_state_->delegate->BeforeWait();
    }
    last_wakeup_was_spurious = false;

    // Tell the optimizer to retain these values to simplify analyzing hangs.
    base::debug::Alias(&delay);
    base::debug::Alias(&wait_flags);
    DWORD result;
    if (g_ui_pump_improvements_win) {
      HANDLE event_handle = event_.handle();
      result = MsgWaitForMultipleObjectsEx(1, &event_handle, delay, QS_ALLINPUT,
                                           wait_flags);
      DPCHECK(WAIT_FAILED != result);
      if (result == WAIT_OBJECT_0) {
        wakeup_state_ = WakeupState::kApplicationTask;
      } else if (result == WAIT_OBJECT_0 + 1) {
        wakeup_state_ = WakeupState::kNative;
      } else {
        wakeup_state_ = WakeupState::kInactive;
      }
    } else {
      result = MsgWaitForMultipleObjectsEx(0, nullptr, delay, QS_ALLINPUT,
                                           wait_flags);
      DPCHECK(WAIT_FAILED != result);
      if (result == WAIT_OBJECT_0) {
        wakeup_state_ = WakeupState::kNative;
      } else {
        wakeup_state_ = WakeupState::kInactive;
      }
    }

    if (wakeup_state_ == WakeupState::kApplicationTask) {
      // This can only be reached when the pump woke up via `event_`. In that
      // case, tasks are prioritized over native.
      return;
    } else if (wakeup_state_ == WakeupState::kNative) {
      // A WM_* message is available.
      // If a parent child relationship exists between windows across threads
      // then their thread inputs are implicitly attached.
      // This causes the MsgWaitForMultipleObjectsEx API to return indicating
      // that messages are ready for processing (Specifically, mouse messages
      // intended for the child window may appear if the child window has
      // capture).
      // The subsequent PeekMessages call may fail to return any messages thus
      // causing us to enter a tight loop at times.
      // The code below is a workaround to give the child window
      // some time to process its input messages by looping back to
      // MsgWaitForMultipleObjectsEx above when there are no messages for the
      // current thread.

      // As in ProcessNextWindowsMessage().
      auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();
      {
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("base"),
                     "MessagePumpForUI::WaitForWork GetQueueStatus");
        if (HIWORD(::GetQueueStatus(QS_SENDMESSAGE)) & QS_SENDMESSAGE)
          return;
      }
      {
        MSG msg;
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("base"),
                     "MessagePumpForUI::WaitForWork PeekMessage");
        if (::PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
          return;
        }
      }

      // We know there are no more messages for this thread because PeekMessage
      // has returned false. Reset |wait_flags| so that we wait for a *new*
      // message.
      wait_flags = 0;
    } else {
      DCHECK_EQ(wakeup_state_, WakeupState::kInactive);
      last_wakeup_was_spurious = true;
      TRACE_EVENT_INSTANT(
          "base", "MessagePumpForUI::WaitForWork Spurious Wakeup",
          [&](perfetto::EventContext ctx) {
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_chrome_message_pump_for_ui()
                ->set_wait_for_object_result(result);
          });
    }
  }
}

void MessagePumpForUI::HandleWorkMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // If we are being called outside of the context of Run, then don't try to do
  // any work.  This could correspond to a MessageBox call or something of that
  // sort.
  if (!run_state_) {
    // Since we handled a kMsgHaveWork message, we must still update this flag.
    native_msg_scheduled_.store(false, std::memory_order_relaxed);
    return;
  }

  // Let whatever would have run had we not been putting messages in the queue
  // run now.  This is an attempt to make our dummy message not starve other
  // messages that may be in the Windows message queue.
  ProcessPumpReplacementMessage();

  Delegate::NextWorkInfo next_work_info = run_state_->delegate->DoWork();
  if (next_work_info.is_immediate()) {
    ScheduleWork();
  } else {
    run_state_->delegate->BeforeWait();
    ScheduleNativeTimer(next_work_info);
  }
}

void MessagePumpForUI::HandleTimerMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // ::KillTimer doesn't remove pending WM_TIMER messages from the queue,
  // explicitly ignore the last WM_TIMER message in that case to avoid handling
  // work from here when DoRunLoop() is active (which could result in scheduling
  // work from two places at once). Note: we're still fine in the event that a
  // second native nested loop is entered before such a dead WM_TIMER message is
  // discarded because ::SetTimer merely resets the timer if invoked twice with
  // the same id.
  if (!installed_native_timer_)
    return;

  // We only need to fire once per specific delay, another timer may be
  // scheduled below but we're done with this one.
  KillNativeTimer();

  // If we are being called outside of the context of Run, then don't do
  // anything.  This could correspond to a MessageBox call or something of
  // that sort.
  if (!run_state_)
    return;

  Delegate::NextWorkInfo next_work_info = run_state_->delegate->DoWork();
  if (next_work_info.is_immediate()) {
    ScheduleWork();
  } else {
    run_state_->delegate->BeforeWait();
    ScheduleNativeTimer(next_work_info);
  }
}

void MessagePumpForUI::ScheduleNativeTimer(
    Delegate::NextWorkInfo next_work_info) {
  DCHECK(!next_work_info.is_immediate());
  // We should only ScheduleNativeTimer() under the new pump implementation
  // while nested with application tasks.
  DCHECK(!g_ui_pump_improvements_win ||
         in_nested_native_loop_with_application_tasks_);

  // Do not redundantly set the same native timer again if it was already set.
  // This can happen when a nested native loop goes idle with pending delayed
  // tasks, then gets woken up by an immediate task, and goes back to idle with
  // the same pending delay. No need to kill the native timer if there is
  // already one but the |delayed_run_time| has changed as ::SetTimer reuses the
  // same id and will replace and reset the existing timer.
  if (installed_native_timer_ &&
      *installed_native_timer_ == next_work_info.delayed_run_time) {
    return;
  }

  if (next_work_info.delayed_run_time.is_max())
    return;

  // We do not use native Windows timers in general as they have a poor, 10ms,
  // granularity. Instead we rely on MsgWaitForMultipleObjectsEx's
  // high-resolution timeout to sleep without timers in WaitForWork(). However,
  // when entering a nested native ::GetMessage() loop (e.g. native modal
  // windows) under a `ScopedAllowApplicationTasksInNativeNestedLoop`, we have
  // to rely on a native timer when HandleWorkMessage() runs out of immediate
  // work. Since `ScopedAllowApplicationTasksInNativeNestedLoop` invokes
  // ScheduleWork() : we are guaranteed that HandleWorkMessage() will be called
  // after entering a nested native loop that should process application
  // tasks. But once HandleWorkMessage() is out of immediate work, ::SetTimer()
  // is used to guarantee we are invoked again should the next delayed task
  // expire before the nested native loop ends. The native timer being
  // unnecessary once we return to our DoRunLoop(), we ::KillTimer when it
  // resumes (nested native loops should be rare so we're not worried about
  // ::SetTimer<=>::KillTimer churn).  TODO(gab): The long-standing legacy
  // dependency on the behavior of
  // `ScopedAllowApplicationTasksInNativeNestedLoop` is unfortunate, would be
  // nice to make this a MessagePump concept (instead of requiring impls to
  // invoke ScheduleWork() one-way and no-op DoWork() the other way).

  UINT delay_msec = strict_cast<UINT>(GetSleepTimeoutMs(
      next_work_info.delayed_run_time, next_work_info.recent_now));
  if (delay_msec == 0) {
    ScheduleWork();
  } else {
    // TODO(gab): ::SetTimer()'s documentation claims it does this for us.
    // Consider removing this safety net.
    delay_msec = std::clamp(delay_msec, static_cast<UINT>(USER_TIMER_MINIMUM),
                            static_cast<UINT>(USER_TIMER_MAXIMUM));

    // Tell the optimizer to retain the delay to simplify analyzing hangs.
    base::debug::Alias(&delay_msec);
    const UINT_PTR ret =
        ::SetTimer(message_window_.hwnd(), reinterpret_cast<UINT_PTR>(this),
                   delay_msec, nullptr);

    if (ret) {
      installed_native_timer_ = next_work_info.delayed_run_time;
      return;
    }
    // This error is likely similar to MESSAGE_POST_ERROR (i.e. native queue is
    // full). Since we only use ScheduleNativeTimer() in native nested loops
    // this likely means this pump will not be given a chance to run application
    // tasks until the nested loop completes.
    TRACE_EVENT_INSTANT0("base", "Chrome.MessageLoopProblem.SET_TIMER_ERROR",
                         TRACE_EVENT_SCOPE_THREAD);
  }
}

void MessagePumpForUI::KillNativeTimer() {
  DCHECK(installed_native_timer_);
  const bool success =
      ::KillTimer(message_window_.hwnd(), reinterpret_cast<UINT_PTR>(this));
  DPCHECK(success);
  installed_native_timer_.reset();
}

bool MessagePumpForUI::ProcessNextWindowsMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  MSG msg;
  bool has_msg = false;
  bool more_work_is_plausible = false;
  {
    // ::PeekMessage() may process sent and/or internal messages (regardless of
    // |had_messages| as ::GetQueueStatus() is an optimistic check that may
    // racily have missed an incoming event -- it doesn't hurt to have empty
    // internal units of work when ::PeekMessage turns out to be a no-op).
    // Instantiate |scoped_do_work| ahead of GetQueueStatus() so that
    // trace events it emits fully outscope GetQueueStatus' events
    // (GetQueueStatus() itself not being expected to do work; it's fine to use
    // only one ScopedDoWorkItem for both calls -- we trace them independently
    // just in case internal work stalls).
    auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();

    {
      // Individually trace ::GetQueueStatus and ::PeekMessage because sampling
      // profiler is hinting that we're spending a surprising amount of time
      // with these on top of the stack. Tracing will be able to tell us whether
      // this is a bias of sampling profiler (e.g. kernel takes ::GetQueueStatus
      // as an opportunity to swap threads and is more likely to schedule the
      // sampling profiler's thread while the sampled thread is swapped out on
      // this frame).
      TRACE_EVENT0(
          TRACE_DISABLED_BY_DEFAULT("base"),
          "MessagePumpForUI::ProcessNextWindowsMessage GetQueueStatus");
      DWORD queue_status = ::GetQueueStatus(QS_SENDMESSAGE);

      // If there are sent messages in the queue then PeekMessage internally
      // dispatches the message and returns false. We return true in this case
      // to ensure that the message loop peeks again instead of calling
      // MsgWaitForMultipleObjectsEx.
      if (HIWORD(queue_status) & QS_SENDMESSAGE)
        more_work_is_plausible = true;
    }

    {
      // PeekMessage can run a message if there are sent messages, trace that
      // and emit the boolean param to see if it ever janks independently (ref.
      // comment on GetQueueStatus).
      TRACE_EVENT(
          TRACE_DISABLED_BY_DEFAULT("base"),
          "MessagePumpForUI::ProcessNextWindowsMessage PeekMessage",
          [&](perfetto::EventContext ctx) {
            perfetto::protos::pbzero::ChromeMessagePump* msg_pump_data =
                ctx.event()->set_chrome_message_pump();
            msg_pump_data->set_sent_messages_in_queue(more_work_is_plausible);
          });
      has_msg = ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE;
    }
  }
  if (has_msg)
    more_work_is_plausible |= ProcessMessageHelper(msg);

  return more_work_is_plausible;
}

bool MessagePumpForUI::ProcessMessageHelper(const MSG& msg) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  if (msg.message == WM_QUIT) {
    // WM_QUIT is the standard way to exit a ::GetMessage() loop. Our
    // MessageLoop has its own quit mechanism, so WM_QUIT is generally
    // unexpected.
    TRACE_EVENT_INSTANT0("base",
                         "Chrome.MessageLoopProblem.RECEIVED_WM_QUIT_ERROR",
                         TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  // While running our main message pump, we discard kMsgHaveWork messages.
  if (msg.message == kMsgHaveWork && msg.hwnd == message_window_.hwnd())
    return ProcessPumpReplacementMessage();

  run_state_->delegate->BeginNativeWorkBeforeDoWork();
  auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();

  TRACE_EVENT("base,toplevel", "MessagePumpForUI DispatchMessage",
              [&](perfetto::EventContext ctx) {
                ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                    ->set_chrome_message_pump_for_ui()
                    ->set_message_id(msg.message);
              });

  for (Observer& observer : observers_)
    observer.WillDispatchMSG(msg);
  ::TranslateMessage(&msg);
  ::DispatchMessage(&msg);
  for (Observer& observer : observers_)
    observer.DidDispatchMSG(msg);

  return true;
}

bool MessagePumpForUI::ProcessPumpReplacementMessage() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // When we encounter a kMsgHaveWork message, this method is called to peek and
  // process a replacement message. The goal is to make the kMsgHaveWork as non-
  // intrusive as possible, even though a continuous stream of such messages are
  // posted. This method carefully peeks a message while there is no chance for
  // a kMsgHaveWork to be pending, then resets the |have_work_| flag (allowing a
  // replacement kMsgHaveWork to possibly be posted), and finally dispatches
  // that peeked replacement. Note that the re-post of kMsgHaveWork may be
  // asynchronous to this thread!!

  MSG msg;
  bool have_message = false;
  {
    // Note: Ideally this call wouldn't process sent-messages (as we already did
    // that in the PeekMessage call that lead to receiving this kMsgHaveWork),
    // but there's no way to specify this (omitting PM_QS_SENDMESSAGE as in
    // crrev.com/791043 doesn't do anything). Hence this call must be considered
    // as a potential work item.
    auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("base"),
                 "MessagePumpForUI::ProcessPumpReplacementMessage PeekMessage");
    have_message = ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE;
  }

  // Expect no message or a message different than kMsgHaveWork.
  DCHECK(!have_message || kMsgHaveWork != msg.message ||
         msg.hwnd != message_window_.hwnd());

  // Since we discarded a kMsgHaveWork message, we must update the flag.
  DCHECK(native_msg_scheduled_.load(std::memory_order_relaxed));
  native_msg_scheduled_.store(false, std::memory_order_relaxed);

  // We don't need a special time slice if we didn't |have_message| to process.
  if (!have_message)
    return false;

  if (msg.message == WM_QUIT) {
    // If we're in a nested ::GetMessage() loop then we must let that loop see
    // the WM_QUIT in order for it to exit. If we're in DoRunLoop then the re-
    // posted WM_QUIT will be either ignored, or handled, by
    // ProcessMessageHelper() called directly from ProcessNextWindowsMessage().
    ::PostQuitMessage(static_cast<int>(msg.wParam));
    // Note: we *must not* ScheduleWork() here as WM_QUIT is a low-priority
    // message on Windows (it is only returned by ::PeekMessage() when idle) :
    // https://blogs.msdn.microsoft.com/oldnewthing/20051104-33/?p=33453. As
    // such posting a kMsgHaveWork message via ScheduleWork() would cause an
    // infinite loop (kMsgHaveWork message handled first means we end up here
    // again and repost WM_QUIT+ScheduleWork() again, etc.). Not leaving a
    // kMsgHaveWork message behind however is also problematic as unwinding
    // multiple layers of nested ::GetMessage() loops can result in starving
    // application tasks. TODO(crbug.com/40595757) : Fix this.

    // The return value is mostly irrelevant but return true like we would after
    // processing a QuitClosure() task.
    return true;
  } else if (msg.message == WM_TIMER &&
             msg.wParam == reinterpret_cast<UINT_PTR>(this)) {
    // This happens when a native nested loop invokes HandleWorkMessage() =>
    // ProcessPumpReplacementMessage() which finds the WM_TIMER message
    // installed by ScheduleNativeTimer(). That message needs to be handled
    // directly as handing it off to ProcessMessageHelper() below would cause an
    // unnecessary ScopedDoWorkItem which may incorrectly lead the Delegate's
    // heuristics to conclude that the DoWork() in HandleTimerMessage() is
    // nested inside a native work item. It's also safe to skip the below
    // ScheduleWork() as it is not mandatory before invoking DoWork() and
    // HandleTimerMessage() handles re-installing the necessary followup
    // messages.
    HandleTimerMessage();
    return true;
  }

  // Guarantee we'll get another time slice in the case where we go into native
  // windows code. This ScheduleWork() may hurt performance a tiny bit when
  // tasks appear very infrequently, but when the event queue is busy, the
  // kMsgHaveWork events get (percentage wise) rarer and rarer.
  ScheduleWork();
  return ProcessMessageHelper(msg);
}

//-----------------------------------------------------------------------------
// MessagePumpForIO public:

MessagePumpForIO::IOContext::IOContext() {
  memset(&overlapped, 0, sizeof(overlapped));
}

MessagePumpForIO::IOHandler::IOHandler(const Location& from_here)
    : io_handler_location_(from_here) {}

MessagePumpForIO::IOHandler::~IOHandler() = default;

MessagePumpForIO::MessagePumpForIO() {
  port_.Set(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr,
                                     reinterpret_cast<ULONG_PTR>(nullptr), 1));
  DCHECK(port_.is_valid());
}

MessagePumpForIO::~MessagePumpForIO() = default;

void MessagePumpForIO::ScheduleWork() {
  // This is the only MessagePumpForIO method which can be called outside of
  // |bound_thread_|.

  bool not_scheduled = false;
  if (!native_msg_scheduled_.compare_exchange_strong(
          not_scheduled, true, std::memory_order_relaxed)) {
    return;  // Work already scheduled.
  }

  // Make sure the MessagePump does some work for us.
  const BOOL ret = ::PostQueuedCompletionStatus(
      port_.get(), 0, reinterpret_cast<ULONG_PTR>(this),
      reinterpret_cast<OVERLAPPED*>(this));
  if (ret)
    return;  // Post worked perfectly.

  // See comment in MessagePumpForUI::ScheduleWork() for this error recovery.

  native_msg_scheduled_.store(
      false, std::memory_order_relaxed);  // Clarify that we didn't succeed.
  TRACE_EVENT_INSTANT0("base",
                       "Chrome.MessageLoopProblem.COMPLETION_POST_ERROR",
                       TRACE_EVENT_SCOPE_THREAD);
}

void MessagePumpForIO::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // Since this is always called from |bound_thread_|, there is nothing to do as
  // the loop is already running. It will WaitForWork() in
  // DoRunLoop() with the correct timeout when it's out of immediate tasks.
}

HRESULT MessagePumpForIO::RegisterIOHandler(HANDLE file_handle,
                                            IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  HANDLE port = ::CreateIoCompletionPort(
      file_handle, port_.get(), reinterpret_cast<ULONG_PTR>(handler), 1);
  return (port != nullptr) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

bool MessagePumpForIO::RegisterJobObject(HANDLE job_handle,
                                         IOHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  JOBOBJECT_ASSOCIATE_COMPLETION_PORT info;
  info.CompletionKey = handler;
  info.CompletionPort = port_.get();
  return ::SetInformationJobObject(job_handle,
                                   JobObjectAssociateCompletionPortInformation,
                                   &info, sizeof(info)) != FALSE;
}

//-----------------------------------------------------------------------------
// MessagePumpForIO private:

void MessagePumpForIO::DoRunLoop() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  for (;;) {
    // If we do any work, we may create more messages etc., and more work may
    // possibly be waiting in another task group.  When we (for example)
    // WaitForIOCompletion(), there is a good chance there are still more
    // messages waiting.  On the other hand, when any of these methods return
    // having done no work, then it is pretty unlikely that calling them
    // again quickly will find any work to do.  Finally, if they all say they
    // had no work, then it is a good time to consider sleeping (waiting) for
    // more work.

    Delegate::NextWorkInfo next_work_info = run_state_->delegate->DoWork();
    bool more_work_is_plausible = next_work_info.is_immediate();
    if (run_state_->should_quit)
      break;

    more_work_is_plausible |= WaitForIOCompletion(0);
    if (run_state_->should_quit)
      break;

    if (more_work_is_plausible)
      continue;

    run_state_->delegate->DoIdleWork();
    if (run_state_->should_quit)
      break;

    run_state_->delegate->BeforeWait();
    WaitForWork(next_work_info);
  }
}

// Wait until IO completes, up to the time needed by the timer manager to fire
// the next set of timers.
void MessagePumpForIO::WaitForWork(Delegate::NextWorkInfo next_work_info) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  // We do not support nested IO message loops. This is to avoid messy
  // recursion problems.
  DCHECK(!run_state_->is_nested) << "Cannot nest an IO message loop!";

  DWORD timeout = GetSleepTimeoutMs(next_work_info.delayed_run_time,
                                    next_work_info.recent_now);

  // Tell the optimizer to retain these values to simplify analyzing hangs.
  base::debug::Alias(&timeout);
  WaitForIOCompletion(timeout);
}

bool MessagePumpForIO::WaitForIOCompletion(DWORD timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  IOItem item;
  if (!GetIOItem(timeout, &item))
    return false;

  if (ProcessInternalIOItem(item))
    return true;

  run_state_->delegate->BeginNativeWorkBeforeDoWork();
  auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();

  TRACE_EVENT(
      "base,toplevel", "IOHandler::OnIOCompleted",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_chrome_message_pump()->set_io_handler_location_iid(
            base::trace_event::InternedSourceLocation::Get(
                &ctx, base::trace_event::TraceSourceLocation(
                          item.handler->io_handler_location())));
      });

  item.handler.ExtractAsDangling()->OnIOCompleted(
      item.context.ExtractAsDangling(), item.bytes_transfered, item.error);

  return true;
}

// Asks the OS for another IO completion result.
bool MessagePumpForIO::GetIOItem(DWORD timeout, IOItem* item) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  ULONG_PTR key = reinterpret_cast<ULONG_PTR>(nullptr);
  OVERLAPPED* overlapped = nullptr;

  // Clear the value for the number of bytes transferred in case extracting the
  // packet doesn't populate it.
  item->bytes_transfered = 0;
  if (!::GetQueuedCompletionStatus(port_.get(), &item->bytes_transfered, &key,
                                   &overlapped, timeout)) {
    if (!overlapped)
      return false;  // Nothing in the queue.
    // A completion packet for a failed operation was processed. The Windows
    // last error code pertains to the operation that failed.
    item->error = ::GetLastError();
    // The packet may have contained a value for the number of bytes
    // transferred, so pass along whatever value was populated from it.
  } else {
    // The packet corresponded to an operation that succeeded, so clear out
    // the error value so that the handler sees the operation as a success.
    item->error = ERROR_SUCCESS;
  }

  item->handler = reinterpret_cast<IOHandler*>(key);
  item->context = reinterpret_cast<IOContext*>(overlapped);
  return true;
}

bool MessagePumpForIO::ProcessInternalIOItem(const IOItem& item) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

  if (reinterpret_cast<void*>(this) ==
          reinterpret_cast<void*>(item.context.get()) &&
      reinterpret_cast<void*>(this) ==
          reinterpret_cast<void*>(item.handler.get())) {
    // This is our internal completion.
    DCHECK(!item.bytes_transfered);
    native_msg_scheduled_.store(false, std::memory_order_relaxed);
    return true;
  }
  return false;
}

}  // namespace base
