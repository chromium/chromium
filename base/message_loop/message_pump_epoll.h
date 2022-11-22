// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_EPOLL_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_EPOLL_H_

#include <sys/epoll.h>

#include <cstdint>
#include <map>

#include "base/base_export.h"
#include "base/containers/stack_container.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {

// A MessagePump implementation suitable for I/O message loops on Linux-based
// systems with epoll API support.
class BASE_EXPORT MessagePumpEpoll : public MessagePump,
                                     public WatchableIOMessagePumpPosix {
  using InterestParams = MessagePumpLibevent::EpollInterestParams;
  using Interest = MessagePumpLibevent::EpollInterest;

 public:
  using FdWatchController = MessagePumpLibevent::FdWatchController;

  MessagePumpEpoll();
  MessagePumpEpoll(const MessagePumpEpoll&) = delete;
  MessagePumpEpoll& operator=(const MessagePumpEpoll&) = delete;
  ~MessagePumpEpoll() override;

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* watcher);

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;

 private:
  friend class MessagePumpLibevent;
  friend class MessagePumpLibeventTest;

  // The WatchFileDescriptor API supports multiple FdWatchControllers watching
  // the same file descriptor, potentially for different events; but the epoll
  // API only supports a single interest list entry per unique file descriptor.
  //
  // EpollEventEntry tracks all epoll state relevant to a single file
  // descriptor, including references to all active and inactive Interests
  // concerned with that descriptor. This is used to derive a single aggregate
  // interest entry for the descriptor when manipulating epoll.
  struct EpollEventEntry {
    explicit EpollEventEntry(int fd);
    EpollEventEntry(const EpollEventEntry&) = delete;
    EpollEventEntry& operator=(const EpollEventEntry&) = delete;
    ~EpollEventEntry();

    static EpollEventEntry& FromEpollEvent(epoll_event& e) {
      return *static_cast<EpollEventEntry*>(e.data.ptr);
    }

    // Returns the combined set of epoll event flags which should be monitored
    // by the epoll instance for `fd`. This is based on a combination of the
    // parameters of all currently active elements in `interests`. Namely:
    //   - EPOLLIN is set if any active Interest wants to `read`.
    //   - EPOLLOUT is set if any active Interest wants to `write`.
    //   - EPOLLONESHOT is set if all active Interests are one-shot.
    uint32_t ComputeActiveEvents();

    // The file descriptor to which this entry pertains.
    const int fd;

    // A cached copy of the last known epoll event bits registered for this
    // descriptor on the epoll instance.
    uint32_t registered_events = 0;

    // A collection of all the interests regarding `fd` on this message pump.
    // The small amount of inline storage avoids heap allocation in virtually
    // all real scenarios, since there's little practical value in having more
    // than two controllers (e.g. one reader and one writer) watch the same
    // descriptor on the same thread.
    StackVector<scoped_refptr<Interest>, 2> interests;

    // Temporary pointer to an active epoll_event structure which refers to
    // this entry. This is set immediately upon returning from epoll_wait() and
    // cleared again immediately before dispatching to any registered interests,
    // so long as this entry isn't destroyed in the interim.
    raw_ptr<epoll_event> active_event = nullptr;
  };

  // State which lives on the stack within Run(), to support nested run loops.
  struct RunState {
    explicit RunState(Delegate* delegate) : delegate(delegate) {}

    // `delegate` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION Delegate* const delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit = false;
  };

  void AddEpollEvent(EpollEventEntry& entry);
  void UpdateEpollEvent(EpollEventEntry& entry);
  void UnregisterInterest(const scoped_refptr<Interest>& interest);
  bool WaitForEpollEvents(TimeDelta timeout);
  void OnEpollEvent(EpollEventEntry& entry, uint32_t events);
  void HandleEvent(int fd,
                   bool can_read,
                   bool can_write,
                   FdWatchController* controller);
  void HandleWakeUp();

  // Null if Run() is not currently executing. Otherwise it's a pointer into the
  // stack of the innermost nested Run() invocation.
  RunState* run_state_ = nullptr;

  // Mapping of all file descriptors currently watched by this message pump.
  // std::map was chosen because (1) the number of elements can vary widely,
  // (2) we don't do frequent lookups, and (3) values need stable addresses
  // across insertion or removal of other elements.
  std::map<int, EpollEventEntry> entries_;

  // The epoll instance used by this message pump to monitor file descriptors.
  ScopedFD epoll_;

  // An eventfd object used to wake the pump's thread when scheduling new work.
  ScopedFD wake_event_;

  // WatchFileDescriptor() must be called from this thread, and so must
  // FdWatchController::StopWatchingFileDescriptor().
  THREAD_CHECKER(thread_checker_);

  WeakPtrFactory<MessagePumpEpoll> weak_ptr_factory_{this};
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_EPOLL_H_
