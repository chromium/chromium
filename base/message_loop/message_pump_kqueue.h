// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_KQUEUE_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_KQUEUE_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/event.h>

#include <vector>

#include "base/containers/id_map.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/mac/scoped_mach_port.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"

namespace base {

// MessagePumpKqueue is used on macOS to drive an IO MessageLoop that is
// capable of watching both POSIX file descriptors and Mach ports.
// MessagePumpKqueue 在 macOS 上用于驱动一个 IO MessageLoop，并同时监听POSIX
// 文件描述符和 Mach端口。
// 本质上是通过kqueue这个IO多路复用内核态机制，来监听Mach端口事件、fd事件，在一个死
// 循环中，如果kqueue中有事件变更，则根据kqueue的变更事件列表中，遍历读取每一个事件，
// 根据事件类型来确定是Match端口 或 fd IO事件，分别回调 MachPortWatchController
// 或 FdWatchController 中的回调函数，
// kqueue是Kernel Queues，参见：https://www.manpagez.com/man/2/kevent64/
//
class BASE_EXPORT MessagePumpKqueue : public MessagePump,
                                      public WatchableIOMessagePumpPosix {
 public:
  class FdWatchController : public FdWatchControllerInterface {
   public:
    explicit FdWatchController(const Location& from_here);

    FdWatchController(const FdWatchController&) = delete;
    FdWatchController& operator=(const FdWatchController&) = delete;

    ~FdWatchController() override;

    // FdWatchControllerInterface:
    bool StopWatchingFileDescriptor() override;

   protected:
    friend class MessagePumpKqueue;

    void Init(WeakPtr<MessagePumpKqueue> pump,
              int fd,
              int mode,
              FdWatcher* watcher);

    void Reset();

    int fd() { return fd_; }
    int mode() { return mode_; }
    FdWatcher* watcher() { return watcher_; }

   private:
    int fd_ = -1;
    int mode_ = 0;
    FdWatcher* watcher_ = nullptr;
    WeakPtr<MessagePumpKqueue> pump_;
  };

  // Delegate interface that provides notifications of Mach
  // message receive events.
  // 提供 Mach 消息接收事件通知的代理接口。
  // Mach内核提供的进程间消息通讯（IPC）的能力。
  class MachPortWatcher {
   public:
    virtual ~MachPortWatcher() {}
    /**
     * @brief 从 mach_port_t 中接收消息
     */
    virtual void OnMachMessageReceived(mach_port_t port) = 0;
  };

  // Controller interface that is used to stop receiving events for an
  // installed MachPortWatcher.
  // 用于停止接收已安装 MachPortWatcher 事件的控制器接口。
  class MachPortWatchController {
   public:
    explicit MachPortWatchController(const Location& from_here);

    MachPortWatchController(const MachPortWatchController&) = delete;
    MachPortWatchController& operator=(const MachPortWatchController&) = delete;

    ~MachPortWatchController();
    // 停止监听 Mach Port
    bool StopWatchingMachPort();

   protected:
    friend class MessagePumpKqueue;

    void Init(WeakPtr<MessagePumpKqueue> pump,
              mach_port_t port,
              MachPortWatcher* watcher);

    void Reset();

    mach_port_t port() {
      return port_;
    }

    MachPortWatcher* watcher() {
      return watcher_;
    }

   private:
    // MACH_PORT_NULL 是可以在消息中携带的合法值。
    // 它表示没有任何端口或端口权限。 （端口参数使消息保持“简单”，
    // 即使值为 MACH_PORT_NULL。）值
    mach_port_t port_ = MACH_PORT_NULL;
    MachPortWatcher* watcher_ = nullptr;
    WeakPtr<MessagePumpKqueue> pump_;
    const Location from_here_;
  };

  MessagePumpKqueue();

  MessagePumpKqueue(const MessagePumpKqueue&) = delete;
  MessagePumpKqueue& operator=(const MessagePumpKqueue&) = delete;

  ~MessagePumpKqueue() override;

  // MessagePump:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

  // Begins watching the Mach receive right named by |port|. The |controller|
  // can be used to stop watching for incoming messages, and new message
  // notifications are delivered to the |delegate|. Returns true if the watch
  // was successfully set-up and false on error.
  // 开始观察 Mach 接收由 |port| 命名的权限。 |controller| 可用于停止监视传入消息，
  // 并将新消息通知传递给 |delegate|。如果手表设置成功，则返回真，错误时返回假。
  bool WatchMachReceivePort(mach_port_t port,
                            MachPortWatchController* controller,
                            MachPortWatcher* delegate);

  // WatchableIOMessagePumpPosix:
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* delegate);

  bool GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting() const;
  void MaybeUpdateWakeupTimerForTesting(const base::TimeTicks& wakeup_time);

 protected:
  // Virtual for testing.
  virtual void SetWakeupTimerEvent(const base::TimeTicks& wakeup_time,
                                   bool use_slack,
                                   kevent64_s* timer_event);

 private:
  // Called by the watch controller implementations to stop watching the
  // respective types of handles.
  bool StopWatchingMachPort(MachPortWatchController* controller);
  bool StopWatchingFileDescriptor(FdWatchController* controller);

  // Checks the |kqueue_| for events. If |next_work_info| is null, then the
  // kqueue will be polled for events. If it is non-null, it will wait for the
  // amount of time specified by the NextWorkInfo or until an event is
  // triggered. Returns whether any events were dispatched, with the events
  // stored in |events_|.
  // 为事件检查 |kqueue_| 。如果 |next_work_info| 为空，则 kqueue 将被轮询事件。
  // 如果它不为空，它将等待 NextWorkInfo 指定的时间量或直到触发事件。 返回是否发送
  // 了任何事件，事件存储在 |events_| 中。
  bool DoInternalWork(Delegate* delegate,
                      Delegate::NextWorkInfo* next_work_info);

  // Called by DoInternalWork() to dispatch the user events stored in |events_|
  // that were triggered. |count| is the number of events to process. Returns
  // true if work was done, or false if no work was done.
  bool ProcessEvents(Delegate* delegate, int count);

  // Updates the wakeup timer to |wakeup_time| if it differs from the currently
  // scheduled wakeup. Clears the wakeup timer if |wakeup_time| is
  // base::TimeTicks::Max().
  // Updates |scheduled_wakeup_time_| to follow.
  void MaybeUpdateWakeupTimer(const base::TimeTicks& wakeup_time);

  // Ludicrous slack is applied when this function returns true.
  bool IsLudicrousTimerSlackEnabledAndNotSuspended() const;

  // Receive right to which an empty Mach message is sent to wake up the pump
  // in response to ScheduleWork().
  // 接收权，向其发送空Mach消息以唤醒泵以响应 ScheduleWork()。
  mac::ScopedMachReceiveRight wakeup_;
  // Scratch buffer that is used to receive the message sent to |wakeup_|.
  // 用于接收发送到 |wakeup_| 的消息的暂存缓冲区
  mach_msg_empty_rcv_t wakeup_buffer_; // 双向队列

  // A Mach port set used to watch ports from WatchMachReceivePort(). This is
  // only used on macOS <10.12, where kqueues cannot watch ports directly.
  // 用于监视来自 WatchMachReceivePort() 的端口的 Mach 端口集。
  // 这仅在 macOS <10.12 上使用，其中 kqueue 无法直接观察端口。
  mac::ScopedMachPortSet port_set_; // Mach Port 集合

  // Watch controllers for FDs. IDs are generated by the map and are stored in
  // the kevent64_s::udata field.
  IDMap<FdWatchController*> fd_controllers_;

  // Watch controllers for Mach ports. IDs are the port being watched.
  IDMap<MachPortWatchController*> port_controllers_;

  // The kqueue that drives the pump.
  ScopedFD kqueue_; // kqueue消息泵的核心字段，kqueue文件描述符

  // Whether the pump has been Quit() or not.
  bool keep_running_ = true;

  // Cache flag for ease of testing.
  const bool is_ludicrous_timer_slack_enabled_;

  // True if Ludicrous slack was suspended last time the wakeup timer was
  // updated.
  bool ludicrous_timer_slack_was_suspended_;

  // The currently scheduled wakeup, if any. If no wakeup is scheduled,
  // contains base::TimeTicks::Max().
  base::TimeTicks scheduled_wakeup_time_{base::TimeTicks::Max()};

  // The number of events scheduled on the |kqueue_|. There is always at least
  // 1, for the |wakeup_| port (or |port_set_|).
  size_t event_count_ = 1;
  // Buffer used by DoInternalWork() to be notified of triggered events. This
  // is always at least |event_count_|-sized.
  // DoInternalWork() 用于通知触发事件的缓冲区。 这总是至少为 |event_count_| 大小。
  std::vector<kevent64_s> events_{event_count_};

  WeakPtrFactory<MessagePumpKqueue> weak_factory_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_KQUEUE_H_
