// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_

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
class BASE_EXPORT MessagePumpForIOEventImpl : public MessagePumpForIO {
 public:
  MessagePumpForIOEventImpl();
  ~MessagePumpForIOEventImpl() override;

  bool SupportsIOEvents() const override;
  bool WatchIOEvent(IOEventWatchController* controller,
                    IOEventHandler* handler) override;
  bool RegisterIOHandler(HANDLE file_handle, IOHandler* handler) override;
  bool RegisterJobObject(HANDLE job_handle, IOHandler* handler) override;

 private:
  friend class MessagePumpForIO::IOEventWatchController;

  bool ScheduleNativeWork() override;
  bool WaitForIOCompletion(DWORD timeout) override;
  void StopWatching(IOEventWatchController* controller);

  WaitableEvent schedule_work_event_{WaitableEvent::ResetPolicy::AUTOMATIC};
  std::vector<raw_ptr<IOEventWatchController>> watches_;
  // Matches `watches_`, followed by the task-wakeup event.
  std::vector<HANDLE> events_{schedule_work_event_.handle()};
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_EVENT_WIN_H_
