// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_

#include <memory>
#include <tuple>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_buildflags.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"
#include "third_party/libevent/event.h"

// Declare structs we need from libevent.h rather than including it
struct event_base;
struct event;
namespace base {

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
BASE_EXPORT BASE_DECLARE_FEATURE(kMessagePumpEpoll);
#endif  // BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)

class MessagePumpEpoll;

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
class BASE_EXPORT MessagePumpLibevent : public MessagePump,
                                        public WatchableIOMessagePumpPosix {
 public:
  class FdWatchController;

  // Parameters used to construct and describe an EpollInterest.
  struct EpollInterestParams {
    // The file descriptor of interest.
    int fd;

    // Indicates an interest in being able to read() from `fd`.
    bool read;

    // Indicates an interest in being able to write() to `fd`.
    bool write;

    // Indicates whether this interest is a one-shot interest, meaning that it
    // must be automatically deactivated every time it triggers an epoll event.
    bool one_shot;

    bool IsEqual(const EpollInterestParams& rhs) const {
      return std::tie(fd, read, write, one_shot) ==
             std::tie(rhs.fd, rhs.read, rhs.write, rhs.one_shot);
    }
  };

  // Represents a single controller's interest in a file descriptor via epoll,
  // and tracks whether that interest is currently active. Though an interest
  // persists as long as its controller is alive and hasn't changed interests,
  // it only participates in epoll waits while active. These objects are only
  // used when MessagePumpLibevent is configured to use the epoll API instead of
  // libevent.
  class EpollInterest : public RefCounted<EpollInterest> {
   public:
    EpollInterest(FdWatchController* controller,
                  const EpollInterestParams& params);
    EpollInterest(const EpollInterest&) = delete;
    EpollInterest& operator=(const EpollInterest&) = delete;

    FdWatchController* controller() { return controller_; }
    const EpollInterestParams& params() const { return params_; }

    bool active() const { return active_; }
    void set_active(bool active) { active_ = active; }

    // Only meaningful between WatchForControllerDestruction() and
    // StopWatchingForControllerDestruction().
    bool was_controller_destroyed() const { return was_controller_destroyed_; }

    void WatchForControllerDestruction() {
      DCHECK_GE(nested_controller_destruction_watchers_, 0);
      if (nested_controller_destruction_watchers_ == 0) {
        DCHECK(!controller_->was_destroyed_);
        controller_->was_destroyed_ = &was_controller_destroyed_;
      } else {
        // If this is a nested event we should already be watching `controller_`
        // for destruction from an outer event handler.
        DCHECK_EQ(controller_->was_destroyed_, &was_controller_destroyed_);
      }
      ++nested_controller_destruction_watchers_;
    }

    void StopWatchingForControllerDestruction() {
      --nested_controller_destruction_watchers_;
      DCHECK_GE(nested_controller_destruction_watchers_, 0);
      if (nested_controller_destruction_watchers_ == 0 &&
          !was_controller_destroyed_) {
        DCHECK_EQ(controller_->was_destroyed_, &was_controller_destroyed_);
        controller_->was_destroyed_ = nullptr;
      }
    }

   private:
    friend class RefCounted<EpollInterest>;
    ~EpollInterest();

    const raw_ptr<FdWatchController, AcrossTasksDanglingUntriaged> controller_;
    const EpollInterestParams params_;
    bool active_ = true;
    bool was_controller_destroyed_ = false;

    // Avoid resetting `controller_->was_destroyed` when nested destruction
    // watchers are active.
    int nested_controller_destruction_watchers_ = 0;
  };

  // Note that this class is used as the FdWatchController for both
  // MessagePumpLibevent *and* MessagePumpEpoll in order to avoid unnecessary
  // code churn during experimentation and eventual transition. Consumers
  // construct their own FdWatchController instances, so switching this type
  // at runtime would require potentially complex logic changes to all
  // consumers.
  class FdWatchController : public FdWatchControllerInterface {
   public:
    explicit FdWatchController(const Location& from_here);

    FdWatchController(const FdWatchController&) = delete;
    FdWatchController& operator=(const FdWatchController&) = delete;

    // Implicitly calls StopWatchingFileDescriptor.
    ~FdWatchController() override;

    // FdWatchControllerInterface:
    bool StopWatchingFileDescriptor() override;

   private:
    friend class MessagePumpEpoll;
    friend class MessagePumpLibevent;
    friend class MessagePumpLibeventTest;

    // Common methods called by both pump implementations.
    void set_watcher(FdWatcher* watcher) { watcher_ = watcher; }

    // Methods called only by MessagePumpLibevent
    void set_libevent_pump(MessagePumpLibevent* pump) { libevent_pump_ = pump; }
    MessagePumpLibevent* libevent_pump() const { return libevent_pump_; }

    void Init(std::unique_ptr<event> e);
    std::unique_ptr<event> ReleaseEvent();

    void OnFileCanReadWithoutBlocking(int fd, MessagePumpLibevent* pump);
    void OnFileCanWriteWithoutBlocking(int fd, MessagePumpLibevent* pump);

    // Methods called only by MessagePumpEpoll
    void set_epoll_pump(WeakPtr<MessagePumpEpoll> pump) {
      epoll_pump_ = std::move(pump);
    }
    const scoped_refptr<EpollInterest>& epoll_interest() const {
      return epoll_interest_;
    }

    // Creates a new Interest described by `params` and adopts it as this
    // controller's exclusive interest. Any prior interest is dropped by the
    // controller and should be unregistered on the MessagePumpEpoll.
    const scoped_refptr<EpollInterest>& AssignEpollInterest(
        const EpollInterestParams& params);

    void OnFdReadable();
    void OnFdWritable();

    // Common state
    raw_ptr<FdWatcher> watcher_ = nullptr;

    // If this pointer is non-null when the FdWatchController is destroyed, the
    // pointee is set to true.
    raw_ptr<bool> was_destroyed_ = nullptr;

    // State used only with libevent
    std::unique_ptr<event> event_;

    // Tests (e.g. FdWatchControllerPosixTest) deliberately make this dangle.
    raw_ptr<MessagePumpLibevent, DisableDanglingPtrDetection> libevent_pump_ =
        nullptr;

    // State used only with epoll
    WeakPtr<MessagePumpEpoll> epoll_pump_;
    scoped_refptr<EpollInterest> epoll_interest_;
  };

  MessagePumpLibevent();

  MessagePumpLibevent(const MessagePumpLibevent&) = delete;
  MessagePumpLibevent& operator=(const MessagePumpLibevent&) = delete;

  ~MessagePumpLibevent() override;

  // Must be called early in process startup, but after FeatureList
  // initialization. This allows MessagePumpLibevent to query and cache the
  // enabled state of any relevant features.
  static void InitializeFeatures();

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* delegate);

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;

 private:
  friend class MessagePumpLibeventTest;

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // Called by libevent to tell us a registered FD can be read/written to.
  static void OnLibeventNotification(int fd, short flags, void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);

  struct RunState {
    explicit RunState(Delegate* delegate_in) : delegate(delegate_in) {}

    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of sampling
    // profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION Delegate* const delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit = false;
  };

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  // If direct use of epoll is enabled, this is the MessagePumpEpoll instance
  // used. In that case, all libevent state below is ignored and unused.
  // Otherwise this is null.
  std::unique_ptr<MessagePumpEpoll> epoll_pump_;
#endif

  raw_ptr<RunState> run_state_ = nullptr;

  // This flag is set if libevent has processed I/O events.
  bool processed_io_events_ = false;

  struct EventBaseFree {
    inline void operator()(event_base* e) const {
      if (e)
        event_base_free(e);
    }
  };
  // Libevent dispatcher.  Watches all sockets registered with it, and sends
  // readiness callbacks when a socket is ready for I/O.
  std::unique_ptr<event_base, EventBaseFree> event_base_{event_base_new()};

  // ... write end; ScheduleWork() writes a single byte to it
  int wakeup_pipe_in_ = -1;
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  int wakeup_pipe_out_ = -1;
  // ... libevent wrapper for read end
  std::unique_ptr<event> wakeup_event_;

  ThreadChecker watch_file_descriptor_caller_checker_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
