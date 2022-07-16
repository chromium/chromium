// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_WATCHABLE_IO_MESSAGE_PUMP_POSIX_H_
#define BASE_MESSAGE_LOOP_WATCHABLE_IO_MESSAGE_PUMP_POSIX_H_

#include "base/location.h"
#include "base/macros.h"

namespace base {

class WatchableIOMessagePumpPosix {
 public:
  // Used with WatchFileDescriptor to asynchronously monitor the I/O readiness
  // of a file descriptor.
  // 与 WatchFileDescriptor 一起使用以异步监视文件描述符的 I/O 准备情况。
  class FdWatcher {
   public:
    virtual void OnFileCanReadWithoutBlocking(int fd) = 0;
    virtual void OnFileCanWriteWithoutBlocking(int fd) = 0;

   protected:
    virtual ~FdWatcher() = default;
  };

  class FdWatchControllerInterface {
   public:
    explicit FdWatchControllerInterface(const Location& from_here);

    FdWatchControllerInterface(const FdWatchControllerInterface&) = delete;
    FdWatchControllerInterface& operator=(const FdWatchControllerInterface&) = delete;

    // Subclasses must call StopWatchingFileDescriptor() in their destructor
    // (this parent class cannot generically do it for them as it must usually
    // be invoked before they destroy their state which happens before the
    // parent destructor is invoked).
    // 子类必须在它们的析构函数中调用 StopWatchingFileDescriptor() （这个父类通常不能为
    // 它们做这件事，因为它通常必须在它们破坏它们的状态之前被调用，这发生在调用父析构函数之前）
    virtual ~FdWatchControllerInterface();

    // NOTE: This method isn't called StopWatching() to avoid confusion with the
    // win32 ObjectWatcher class. While this doesn't really need to be virtual
    // as there's only one impl per platform and users don't use pointers to the
    // base class. Having this interface forces implementers to share similar
    // implementations (a problem in the past).

    // Stop watching the FD, always safe to call.  No-op if there's nothing to
    // do.
    virtual bool StopWatchingFileDescriptor() = 0;

    const Location& created_from_location() const {
      return created_from_location_;
    }

   private:
    const Location created_from_location_;
  };

  enum Mode {
    WATCH_READ = 1 << 0,
    WATCH_WRITE = 1 << 1,
    WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
  };

  // Every subclass of WatchableIOMessagePumpPosix must provide a
  // WatchFileDescriptor() which has the following signature where
  // |FdWatchController| must be the complete type based on
  // FdWatchControllerInterface.

  // Registers |delegate| with the current thread's message loop so that its
  // methods are invoked when file descriptor |fd| becomes ready for reading or
  // writing (or both) without blocking.  |mode| selects ready for reading, for
  // writing, or both.  See "enum Mode" above.  |controller| manages the
  // lifetime of registrations. ("Registrations" are also ambiguously called
  // "events" in many places, for instance in libevent.)  It is an error to use
  // the same |controller| for different file descriptors; however, the same
  // controller can be reused to add registrations with a different |mode|.  If
  // |controller| is already attached to one or more registrations, the new
  // registration is added onto those.  If an error occurs while calling this
  // method, any registration previously attached to |controller| is removed.
  // Returns true on success.  Must be called on the same thread the MessagePump
  // is running on.
  // bool WatchFileDescriptor(int fd,
  //                          bool persistent,
  //                          int mode,
  //                          FdWatchController* controller,
  //                          FdWatcher* delegate) = 0;
  // 注册 |代理| 使用当前线程的消息循环，以便在文件描述符 |fd| 时调用其方法 准备好
  // 读取或写入（或两者）而不会阻塞。 |模式| 选择准备阅读，准备写作或两者兼而有之。
  // 请参阅上面的“枚举模式”。 |控制器| 管理注册的生命周期。（“注册”在很多地方也被含
  // 糊地称为“事件”，例如在 libevent 中。）使用相同的 |controller| 是错误的。
  // 对于不同的文件描述符； 但是，可以重复使用同一个控制器来添加具有不同 |mode| 的
  // 注册。 如果 |控制器| 已附加到一个或多个注册，新注册将添加到这些注册。 如果调
  // 用此方法时发生错误，之前附加到 |controller| 的任何注册 已移除。 成功时返回真。
  // 必须在运行 MessagePump 的同一线程上调用。
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_WATCHABLE_IO_MESSAGE_PUMP_POSIX_H_
