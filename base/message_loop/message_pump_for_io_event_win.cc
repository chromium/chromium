// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_for_io_event_win.h"

#include <windows.h>

#include <stddef.h>

#include <algorithm>
#include <atomic>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/message_loop/message_pump_wakeup_counter.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/win/scoped_handle.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_message_pump.pbzero.h"

namespace base {

MessagePumpForIO::IOEventHandler::IOEventHandler(const Location& from_here)
    : io_handler_location_(from_here) {}

MessagePumpForIO::IOEventWatchController::IOEventWatchController(
    win::ScopedHandle event_handle)
    : event_(std::move(event_handle)) {
  event_.declare_only_used_while_idle();
}

MessagePumpForIO::IOEventWatchController::~IOEventWatchController() {
  StopWatching();
}

HANDLE MessagePumpForIO::IOEventWatchController::event_handle() const {
  return event_.handle();
}

void MessagePumpForIO::IOEventWatchController::StopWatching() {
  if (pump_) {
    pump_->StopWatching(this);
  }
}

MessagePumpForIOEventImpl::MessagePumpForIOEventImpl() {
  schedule_work_event_.declare_only_used_while_idle();
}

MessagePumpForIOEventImpl::~MessagePumpForIOEventImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  for (IOEventWatchController* controller : watches_) {
    controller->pump_ = nullptr;
    controller->handler_ = nullptr;
  }
}

bool MessagePumpForIOEventImpl::ScheduleNativeWork() {
  schedule_work_event_.Signal();
  return true;
}

bool MessagePumpForIOEventImpl::SupportsIOEvents() const {
  return true;
}

bool MessagePumpForIOEventImpl::WatchIOEvent(IOEventWatchController* controller,
                                             IOEventHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  CHECK(controller);
  CHECK(handler);
  CHECK(!controller->pump_);
  if (watches_.size() == MAXIMUM_WAIT_OBJECTS - 1) {
    SCOPED_CRASH_KEY_NUMBER("MessagePumpForIO", "event_watch_count",
                            watches_.size());
    debug::DumpWithoutCrashing();
    return false;
  }
  controller->pump_ = this;
  controller->handler_ = handler;
  events_.back() = controller->event_handle();
  watches_.push_back(controller);
  // Tasks run between callbacks, so give ready I/O priority over task wakeups.
  events_.push_back(schedule_work_event_.handle());
  return true;
}

bool MessagePumpForIOEventImpl::RegisterIOHandler(HANDLE file_handle,
                                                  IOHandler* handler) {
  // IOCP registration is unavailable in an event-based I/O pump.
  NOTREACHED();
}

bool MessagePumpForIOEventImpl::RegisterJobObject(HANDLE job_handle,
                                                  IOHandler* handler) {
  // Job registration requires an I/O completion port.
  NOTREACHED();
}

void MessagePumpForIOEventImpl::StopWatching(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  const auto watch = std::ranges::find(watches_, controller);
  CHECK(watch != watches_.end());
  const auto index = watch - watches_.begin();
  events_.erase(events_.begin() + index);
  watches_.erase(watch);
  controller->pump_ = nullptr;
  controller->handler_ = nullptr;
}

bool MessagePumpForIOEventImpl::WaitForIOCompletion(DWORD timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  // A native-I/O poll must not consume task wakeups.
  const size_t count = watches_.size() + (timeout ? 1u : 0u);
  if (count == 0) {
    return false;
  }
  const DWORD result = ::WaitForMultipleObjects(checked_cast<DWORD>(count),
                                                events_.data(), FALSE, timeout);
  if (result == WAIT_TIMEOUT) {
    return false;
  }
  PCHECK(result != WAIT_FAILED);
  CHECK_LT(result - WAIT_OBJECT_0, count);
  MessagePumpWakeupCounter::GetForCurrentThread().RecordWakeup();
  if (result - WAIT_OBJECT_0 == watches_.size()) {
    native_msg_scheduled_.store(false, std::memory_order_relaxed);
    return true;
  }

  const size_t index = result - WAIT_OBJECT_0;
  IOEventWatchController* controller = watches_[index];
  IOEventHandler* handler = controller->handler_;

  // Move the signaled watch to the back so a busy event cannot starve others.
  auto watches = span(watches_).subspan(index);
  std::ranges::rotate(watches, watches.begin() + 1);
  auto events = span(events_).subspan(index, watches.size());
  std::ranges::rotate(events, events.begin() + 1);

  run_state_->delegate->BeginNativeWorkBeforeDoWork();
  auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();
  TRACE_EVENT(
      "base,toplevel", "IOEventHandler::OnIOEventSignaled",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_chrome_message_pump()->set_io_handler_location_iid(
            trace_event::InternedSourceLocation::Get(
                &ctx, trace_event::TraceSourceLocation(
                          handler->io_handler_location())));
      });
  handler->OnIOEventSignaled(controller);
  return true;
}

}  // namespace base
