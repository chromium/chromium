// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_H_

#include "base/base_export.h"
#include "base/mac/scoped_cffiledescriptorref.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_mac.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"

namespace base {

// This file introduces a class to monitor sockets and issue callbacks when
// sockets are ready for I/O on iOS.
// 该文件引入了一个类来监视套接字并在套接字准备好在 iOS 上进行 I/O 时发出回调。
// iOS采用的是 CFRunLoop 机制来监听IO事件.
// CFRunLoop对象负责监控事件输入源以及对其进行分发管理。
// CFRunLoop管理的类型通常分为三种类型：
// 1. sources(CFRunLoopSource)
// 2. timers(CFRunLoopTimer)
// 3. observers(CFRunLoopObserver)
class BASE_EXPORT MessagePumpIOSForIO : public MessagePumpNSRunLoop,
                                        public WatchableIOMessagePumpPosix {
 public:
  // IO事件监控控制器：提供callback
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
    friend class MessagePumpIOSForIO;
    friend class MessagePumpIOSForIOTest;

    // Called by MessagePumpIOSForIO, ownership of |fdref| and |fd_source|
    // is transferred to this object.
    void Init(CFFileDescriptorRef fdref,
              CFOptionFlags callback_types,
              CFRunLoopSourceRef fd_source,
              bool is_persistent);

    void set_pump(base::WeakPtr<MessagePumpIOSForIO> pump) { pump_ = pump; }
    const base::WeakPtr<MessagePumpIOSForIO>& pump() const { return pump_; }

    void set_watcher(FdWatcher* watcher) { watcher_ = watcher; }

    void OnFileCanReadWithoutBlocking(int fd, MessagePumpIOSForIO* pump);
    void OnFileCanWriteWithoutBlocking(int fd, MessagePumpIOSForIO* pump);

    bool is_persistent_ = false;  // false if this event is one-shot.
    base::mac::ScopedCFFileDescriptorRef fdref_;
    CFOptionFlags callback_types_ = 0;

    // CFRunLoopSourceRef 是产生事件的地方。Source包括Source0和Source1两个版本，
    // 1. Source0：主要由应用程序管理，它并不能主动触发事件。使用时，你需要先调用
    // CFRunLoopSourceSignal(source)，将这个Source标记为待处理，然后手动调用
    // CFRunLoopWakeUp(runloop)来唤醒RunLoop，让其处理这个事件。通常我们使用
    // 的也是Source0事件。
    // 2. Source1：主要由于RunLoop和kernel进行管理。包含了一个mach_port和一个
    // 回调（函数指针），被用于通过内核和其他线程相互发送消息。这种Source能主动唤
    // 醒RunLoop的线程。
    // 这里IO监听，采用的是 Source1.
    base::ScopedCFTypeRef<CFRunLoopSourceRef> fd_source_;
    base::WeakPtr<MessagePumpIOSForIO> pump_;
    FdWatcher* watcher_ = nullptr;
  };

  MessagePumpIOSForIO();

  MessagePumpIOSForIO(const MessagePumpIOSForIO&) = delete;
  MessagePumpIOSForIO& operator=(const MessagePumpIOSForIO&) = delete;

  ~MessagePumpIOSForIO() override;

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* delegate);

  void RemoveRunLoopSource(CFRunLoopSourceRef source);

 private:
  friend class MessagePumpIOSForIOTest;

  static void HandleFdIOEvent(CFFileDescriptorRef fdref,
                              CFOptionFlags callback_types,
                              void* context);

  ThreadChecker watch_file_descriptor_caller_checker_;

  base::WeakPtrFactory<MessagePumpIOSForIO> weak_factory_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_IO_IOS_H_
