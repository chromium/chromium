// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_win.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/windows_types.h"

namespace base {

// Windows I/O pump that dispatches overlapped I/O through completion events
// instead of an I/O completion port. Intended for Windows Basic Processes,
// where I/O completion ports are unavailable. Consumers register completion
// events with WatchIOEvent(); IOCP and job-object registration are unsupported.
//
// Because Win32's WaitForMultipleObjects() is limited to MAXIMUM_WAIT_OBJECTS
// (64) handles per call, the pump waits directly on up to `kMaxDirectWatches`
// (62) I/O events on the pump thread, reserving one slot for
// `schedule_work_event_` and one slot for `ShardState`'s aggregate completion
// event. Any additional watches beyond `kMaxDirectWatches` are delegated to
// `ShardState`, which manages one or more `WaitShard` worker threads that each
// wait on up to `kMaxShardWatches` (63) events and forward signaled completions
// back to the pump thread.
class BASE_EXPORT MessagePumpForIOEventImpl : public MessagePumpForIO {
 public:
  // Maximum number of I/O event watches waited on directly by the pump thread
  // (reserving two of the 64 WaitForMultipleObjects slots for scheduled work
  // and the aggregate shard-completion event).
  static constexpr size_t kMaxDirectWatches = 64 - 2;
  // Maximum number of I/O event watches waited on by each `WaitShard` worker
  // thread (reserving one of the 64 WaitForMultipleObjects slots for the
  // shard's configuration-changed wakeup event).
  static constexpr size_t kMaxShardWatches = 64 - 1;

  MessagePumpForIOEventImpl();
  ~MessagePumpForIOEventImpl() override;

  bool SupportsIOEvents() const override;
  bool WatchIOEvent(IOEventWatchController* controller,
                    IOEventHandler* handler) override;
  bool RegisterIOHandler(HANDLE file_handle, IOHandler* handler) override;
  bool RegisterJobObject(HANDLE job_handle, IOHandler* handler) override;

  // Returns the number of active or retained `WaitShard` worker threads.
  size_t GetShardCountForTesting() const;

  // Returns the number of sharded completions queued for dispatch.
  size_t GetPendingShardCompletionCountForTesting() const;

 private:
  friend class MessagePumpForIO::IOEventWatchController;

  class ShardState;
  class WaitShard;

  bool ScheduleNativeWork() override;
  bool WaitForIOCompletion(DWORD timeout) override;

  // Polls the shards for a completed I/O whose `WaitShard` worker has not yet
  // forwarded it, so the pump does not go idle with I/O ready. Only called
  // when the run loop would otherwise go idle, keeping this O(shards) cost off
  // the per-task path.
  bool PollForIOBeforeIdle() override;

  // Moves the watch at `index` to the back of `watches_`/`events_` and invokes
  // `controller`'s handler.
  void DispatchIOEvent(size_t index, IOEventWatchController* controller);

  void StopWatching(IOEventWatchController* controller);

  // Signaled by ScheduleNativeWork() to wake the pump thread when immediate or
  // delayed tasks are scheduled.
  WaitableEvent schedule_work_event_{WaitableEvent::ResetPolicy::AUTOMATIC};

  // Controllers corresponding 1:1 with the leading entries of `events_`. A
  // nullptr entry represents `shard_state_->completion_available_handle()`.
  std::vector<raw_ptr<IOEventWatchController>> watches_;

  // Handle array passed to WaitForMultipleObjects(). Contains one handle per
  // entry in `watches_`, followed by `schedule_work_event_.handle()` as the
  // final element.
  std::vector<HANDLE> events_{schedule_work_event_.handle()};

  // Lazily created when the number of watched events exceeds
  // `kMaxDirectWatches`. Once created, its aggregate completion event remains
  // registered in `watches_`/`events_` for the lifetime of the pump.
  std::unique_ptr<ShardState> shard_state_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_
