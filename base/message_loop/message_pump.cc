// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include "base/check.h"
#include "base/message_loop/io_watcher.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/notreached.h"
#include "base/task/current_thread.h"
#include "base/task/task_features.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/message_loop/message_pump_apple.h"
#endif

namespace base {

namespace {

constexpr uint64_t kAlignWakeUpsMask = 1;
constexpr uint64_t kLeewayOffset = 1;

constexpr uint64_t PackAlignWakeUpsAndLeeway(bool align_wake_ups,
                                             TimeDelta leeway) {
  return (static_cast<uint64_t>(leeway.InMilliseconds()) << kLeewayOffset) |
         (align_wake_ups ? kAlignWakeUpsMask : 0);
}

// This stores the current state of |kAlignWakeUps| and leeway. The last bit
// represents if |kAlignWakeUps| is enabled, and the other bits represent the
// leeway value applied to delayed tasks in milliseconds. An atomic is used here
// because the value is queried from multiple threads.
std::atomic<uint64_t> g_align_wake_ups_and_leeway =
    PackAlignWakeUpsAndLeeway(false, kDefaultLeeway);
#if BUILDFLAG(IS_WIN)
bool g_explicit_high_resolution_timer_win = true;
#endif  // BUILDFLAG(IS_WIN)

MessagePump::MessagePumpFactory* message_pump_for_ui_factory_ = nullptr;

#if !BUILDFLAG(IS_NACL) && BUILDFLAG(IS_POSIX)
class MessagePumpForIOFdWatchImpl : public IOWatcher::FdWatch,
                                    public MessagePumpForIO::FdWatcher {
 public:
  MessagePumpForIOFdWatchImpl(IOWatcher::FdWatcher* fd_watcher,
                              const Location& location)
      : fd_watcher_(fd_watcher), controller_(location) {}

  ~MessagePumpForIOFdWatchImpl() override {
    controller_.StopWatchingFileDescriptor();
  }

  MessagePumpForIO::FdWatchController& controller() { return controller_; }

 private:
  // MessagePumpForIO::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    fd_watcher_->OnFdReadable(fd);
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    fd_watcher_->OnFdWritable(fd);
  }

  const raw_ptr<IOWatcher::FdWatcher> fd_watcher_;
  MessagePumpForIO::FdWatchController controller_;
};
#endif

class IOWatcherForCurrentIOThread : public IOWatcher {
 public:
  IOWatcherForCurrentIOThread() : thread_(CurrentIOThread::Get()) {}

  // IOWatcher:
#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_WIN)
  bool RegisterIOHandlerImpl(HANDLE file,
                             MessagePumpForIO::IOHandler* handler) override {
    return thread_.RegisterIOHandler(file, handler);
  }

  bool RegisterJobObjectImpl(HANDLE job,
                             MessagePumpForIO::IOHandler* handler) override {
    return thread_.RegisterJobObject(job, handler);
  }
#elif BUILDFLAG(IS_POSIX)
  std::unique_ptr<FdWatch> WatchFileDescriptorImpl(
      int fd,
      FdWatchDuration duration,
      FdWatchMode mode,
      FdWatcher& fd_watcher,
      const Location& location) override {
    MessagePumpForIO::Mode io_mode;
    switch (mode) {
      case FdWatchMode::kRead:
        io_mode = MessagePumpForIO::WATCH_READ;
        break;
      case FdWatchMode::kWrite:
        io_mode = MessagePumpForIO::WATCH_WRITE;
        break;
      case FdWatchMode::kReadWrite:
        io_mode = MessagePumpForIO::WATCH_READ_WRITE;
        break;
    }
    const bool is_persistent = duration == FdWatchDuration::kPersistent;
    auto watch =
        std::make_unique<MessagePumpForIOFdWatchImpl>(&fd_watcher, location);
    if (!thread_.WatchFileDescriptor(fd, is_persistent, io_mode,
                                     &watch->controller(), watch.get())) {
      return nullptr;
    }
    return watch;
  }
#endif
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && !BUILDFLAG(CRONET_BUILD))
  bool WatchMachReceivePortImpl(
      mach_port_t port,
      MessagePumpForIO::MachPortWatchController* controller,
      MessagePumpForIO::MachPortWatcher* delegate) override {
    return thread_.WatchMachReceivePort(port, controller, delegate);
  }
#elif BUILDFLAG(IS_FUCHSIA)
  bool WatchZxHandleImpl(zx_handle_t handle,
                         bool persistent,
                         zx_signals_t signals,
                         MessagePumpForIO::ZxHandleWatchController* controller,
                         MessagePumpForIO::ZxHandleWatcher* delegate) override {
    return thread_.WatchZxHandle(handle, persistent, signals, controller,
                                 delegate);
  }
#endif  // BUILDFLAG(IS_FUCHSIA)
#endif  // !BUILDFLAG(IS_NACL)

 private:
  CurrentIOThread thread_;
};

}  // namespace

MessagePump::MessagePump() = default;

MessagePump::~MessagePump() = default;

bool MessagePump::HandleNestedNativeLoopWithApplicationTasks(
    bool application_tasks_desired) {
  return false;
}

// static
void MessagePump::OverrideMessagePumpForUIFactory(MessagePumpFactory* factory) {
  DCHECK(!message_pump_for_ui_factory_);
  message_pump_for_ui_factory_ = factory;
}

// static
bool MessagePump::IsMessagePumpForUIFactoryOveridden() {
  return message_pump_for_ui_factory_ != nullptr;
}

// static
std::unique_ptr<MessagePump> MessagePump::Create(MessagePumpType type) {
  switch (type) {
    case MessagePumpType::UI:
      if (message_pump_for_ui_factory_)
        return message_pump_for_ui_factory_();
#if BUILDFLAG(IS_APPLE)
      return message_pump_apple::Create();
#elif BUILDFLAG(IS_NACL) || BUILDFLAG(IS_AIX)
      // Currently NaCl and AIX don't have a UI MessagePump.
      // TODO(abarth): Figure out if we need this.
      NOTREACHED();
#elif BUILDFLAG(IS_ANDROID)
      {
        auto message_pump = std::make_unique<MessagePumpAndroid>();
        message_pump->set_is_type_ui(true);
        return message_pump;
      }
#else
      return std::make_unique<MessagePumpForUI>();
#endif

    case MessagePumpType::IO:
      return std::make_unique<MessagePumpForIO>();

#if BUILDFLAG(IS_ANDROID)
    case MessagePumpType::JAVA:
      return std::make_unique<MessagePumpAndroid>();
#endif

#if BUILDFLAG(IS_APPLE)
    case MessagePumpType::NS_RUNLOOP:
      return std::make_unique<MessagePumpNSRunLoop>();
#endif

    case MessagePumpType::CUSTOM:
      NOTREACHED();

    case MessagePumpType::DEFAULT:
#if BUILDFLAG(IS_IOS)
      // On iOS, a native runloop is always required to pump system work.
      return std::make_unique<MessagePumpCFRunLoop>();
#else
      return std::make_unique<MessagePumpDefault>();
#endif
  }
}

// static
void MessagePump::InitializeFeatures() {
  ResetAlignWakeUpsState();
#if BUILDFLAG(IS_WIN)
  g_explicit_high_resolution_timer_win =
      FeatureList::IsEnabled(kExplicitHighResolutionTimerWin);
  MessagePumpWin::InitializeFeatures();
#elif BUILDFLAG(IS_ANDROID)
  MessagePumpAndroid::InitializeFeatures();
#endif
}

// static
void MessagePump::OverrideAlignWakeUpsState(bool enabled, TimeDelta leeway) {
  g_align_wake_ups_and_leeway.store(PackAlignWakeUpsAndLeeway(enabled, leeway),
                                    std::memory_order_relaxed);
}

// static
void MessagePump::ResetAlignWakeUpsState() {
  OverrideAlignWakeUpsState(FeatureList::IsEnabled(kAlignWakeUps),
                            kTaskLeewayParam.Get());
}

// static
bool MessagePump::GetAlignWakeUpsEnabled() {
  return g_align_wake_ups_and_leeway.load(std::memory_order_relaxed) &
         kAlignWakeUpsMask;
}

// static
TimeDelta MessagePump::GetLeewayIgnoringThreadOverride() {
  return Milliseconds(
      g_align_wake_ups_and_leeway.load(std::memory_order_relaxed) >>
      kLeewayOffset);
}

// static
TimeDelta MessagePump::GetLeewayForCurrentThread() {
  // For some threads, there might be an override of the leeway, so check it
  // first.
  auto leeway_override = PlatformThread::GetThreadLeewayOverride();
  if (leeway_override.has_value()) {
    return leeway_override.value();
  }
  return GetLeewayIgnoringThreadOverride();
}

TimeTicks MessagePump::AdjustDelayedRunTime(TimeTicks earliest_time,
                                            TimeTicks run_time,
                                            TimeTicks latest_time) {
  // Windows relies on the low resolution timer rather than manual wake up
  // alignment when the leeway is less than the OS default timer resolution.
#if BUILDFLAG(IS_WIN)
  if (g_explicit_high_resolution_timer_win &&
      GetLeewayForCurrentThread() <=
          Milliseconds(Time::kMinLowResolutionThresholdMs)) {
    return earliest_time;
  }
#endif  // BUILDFLAG(IS_WIN)
  if (GetAlignWakeUpsEnabled()) {
    TimeTicks aligned_run_time = earliest_time.SnappedToNextTick(
        TimeTicks(), GetLeewayForCurrentThread());
    return std::min(aligned_run_time, latest_time);
  }
  return run_time;
}

IOWatcher* MessagePump::GetIOWatcher() {
  // By default only "IO thread" message pumps support async IO.
  //
  // TODO(crbug.com/379190028): This is done for convenience given the
  // preexistence of CurrentIOThread, but we should eventually remove this in
  // favor of each IO MessagePump implementation defining their own override.
  if (!io_watcher_ && CurrentIOThread::IsSet()) {
    io_watcher_ = std::make_unique<IOWatcherForCurrentIOThread>();
  }
  return io_watcher_.get();
}

}  // namespace base
