// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_LIBDISPATCH_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_LIBDISPATCH_H_

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include <atomic>
#include <memory>

#include "base/apple/dispatch_source.h"
#include "base/base_export.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"

namespace base {

// This file introduces a class to monitor sockets and issue callbacks when
// sockets are ready for I/O on iOS using libdispatch as the backing
// monitoring service.
class BASE_EXPORT MessagePumpIOSForIOLibdispatch
    : public MessagePumpNSRunLoop,
      public WatchableIOMessagePumpPosix {
 public:
  class FdWatchController : public FdWatchControllerInterface {
   public:
    explicit FdWatchController(const Location& location);

    FdWatchController(const FdWatchController&) = delete;
    FdWatchController& operator=(const FdWatchController&) = delete;

    // Implicitly calls StopWatchingFileDescriptor.
    ~FdWatchController() override;

    // FdWatchControllerInterface:
    bool StopWatchingFileDescriptor() override;

   private:
    friend class MessagePumpIOSForIOLibdispatch;
    friend class MessagePumpIOSForIOLibdispatchFdTest;

    void Init(
        const scoped_refptr<base::SequencedTaskRunner>& io_thread_task_runner,
        dispatch_queue_t queue,
        int fd,
        bool persistent,
        int mode,
        FdWatcher* watcher);

    void HandleRead();
    void HandleWrite();
    int fd() { return fd_; }

    bool is_persistent_ = false;  // false if this event is one-shot.
    raw_ptr<FdWatcher> watcher_ = nullptr;
    std::atomic<int> fd_ = -1;
    std::unique_ptr<apple::DispatchSource> dispatch_source_read_;
    std::unique_ptr<apple::DispatchSource> dispatch_source_write_;
    scoped_refptr<SequencedTaskRunner> io_thread_task_runner_;

    base::WeakPtrFactory<FdWatchController> weak_factory_{this};
  };

  // Delegate interface that provides notifications of Mach message receive
  // events.
  class MachPortWatcher {
   public:
    virtual ~MachPortWatcher() = default;
    virtual void OnMachMessageReceived(mach_port_t port) = 0;
  };

  // Controller interface that is used to stop receiving events for an
  // installed MachPortWatcher.
  class MachPortWatchController {
   public:
    explicit MachPortWatchController(const Location& location);

    MachPortWatchController(const MachPortWatchController&) = delete;
    MachPortWatchController& operator=(const MachPortWatchController&) = delete;

    ~MachPortWatchController();

    bool StopWatchingMachPort();

   protected:
    friend class MessagePumpIOSForIOLibdispatch;

    void Init(
        const scoped_refptr<base::SequencedTaskRunner>& io_thread_task_runner,
        dispatch_queue_t queue,
        mach_port_t port,
        MachPortWatcher* watcher);

    void HandleReceive();

   private:
    std::atomic<mach_port_t> port_ = MACH_PORT_NULL;
    raw_ptr<MachPortWatcher> watcher_ = nullptr;
    std::unique_ptr<apple::DispatchSource> dispatch_source_;
    scoped_refptr<SequencedTaskRunner> io_thread_task_runner_;

    base::WeakPtrFactory<MachPortWatchController> weak_factory_{this};
  };

  MessagePumpIOSForIOLibdispatch();

  MessagePumpIOSForIOLibdispatch(const MessagePumpIOSForIOLibdispatch&) =
      delete;
  MessagePumpIOSForIOLibdispatch& operator=(
      const MessagePumpIOSForIOLibdispatch&) = delete;

  ~MessagePumpIOSForIOLibdispatch() override;

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* watcher);

  // Begins watching the Mach receive right named by `port`. The `controller`
  // can be used to stop watching for incoming messages, and new message
  // notifications are delivered to the `watcher`. This implementation always
  // returns true.
  bool WatchMachReceivePort(mach_port_t port,
                            MachPortWatchController* controller,
                            MachPortWatcher* watcher);

 private:
  friend class MessagePumpIOSForIOLibdispatchFdTest;

  THREAD_CHECKER(thread_checker_);
  dispatch_queue_t queue_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_LIBDISPATCH_H_
