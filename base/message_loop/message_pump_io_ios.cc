// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_io_ios.h"

#include "base/notreached.h"

namespace base {

MessagePumpIOSForIO::FdWatchController::FdWatchController(
    const Location& from_here)
    : FdWatchControllerInterface(from_here) {}

MessagePumpIOSForIO::FdWatchController::~FdWatchController() {
  StopWatchingFileDescriptor();
}

bool MessagePumpIOSForIO::FdWatchController::StopWatchingFileDescriptor() {
  if (fdref_ == NULL)
    return true;

  CFFileDescriptorDisableCallBacks(fdref_.get(), callback_types_);
  if (pump_)
    pump_->RemoveRunLoopSource(fd_source_);
  fd_source_.reset();
  fdref_.reset();
  callback_types_ = 0;
  pump_.reset();
  watcher_ = NULL;
  return true;
}

void MessagePumpIOSForIO::FdWatchController::Init(CFFileDescriptorRef fdref,
                                                  CFOptionFlags callback_types,
                                                  CFRunLoopSourceRef fd_source,
                                                  bool is_persistent) {
  DCHECK(fdref);
  DCHECK(!fdref_.is_valid());

  is_persistent_ = is_persistent;
  fdref_.reset(fdref);
  callback_types_ = callback_types;
  fd_source_.reset(fd_source);
}

void MessagePumpIOSForIO::FdWatchController::OnFileCanReadWithoutBlocking(
    int fd,
    MessagePumpIOSForIO* pump) {
  DCHECK(callback_types_ & kCFFileDescriptorReadCallBack);
  watcher_->OnFileCanReadWithoutBlocking(fd);
}

void MessagePumpIOSForIO::FdWatchController::OnFileCanWriteWithoutBlocking(
    int fd,
    MessagePumpIOSForIO* pump) {
  DCHECK(callback_types_ & kCFFileDescriptorWriteCallBack);
  watcher_->OnFileCanWriteWithoutBlocking(fd);
}

MessagePumpIOSForIO::MessagePumpIOSForIO() : weak_factory_(this) {}

MessagePumpIOSForIO::~MessagePumpIOSForIO() {}

bool MessagePumpIOSForIO::WatchFileDescriptor(int fd,
                                              bool persistent,
                                              int mode,
                                              FdWatchController* controller,
                                              FdWatcher* delegate) {
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);

  // WatchFileDescriptor should be called on the pump thread. It is not
  // threadsafe, and your watcher may never be registered.
  DCHECK(watch_file_descriptor_caller_checker_.CalledOnValidThread());

  // 设置fd上下文：
  // typedef struct {
  //   CFIndex	version;
  //   void*	info;
  //   void*	(*retain)(void *info); // 创建策略
  //   void	(*release)(void *info);  // 释放策略
  //   CFStringRef	(*copyDescription)(void *info);
  // } CFFileDescriptorContext;
  CFFileDescriptorContext source_context = {0};
  // 这个字段会传递到回调函数(HandleFdIOEvent)中使用，用于回调IO事件到上层使用方
  source_context.info = controller;

  // 设置感兴趣的可读/写标识，回调类型
  CFOptionFlags callback_types = 0;
  if (mode & WATCH_READ) {
    callback_types |= kCFFileDescriptorReadCallBack;
  }
  if (mode & WATCH_WRITE) {
    callback_types |= kCFFileDescriptorWriteCallBack;
  }

  // 设置文件描述符
  CFFileDescriptorRef fdref = controller->fdref_.get();
  if (fdref == NULL) {
     // 创建文件描述符，并设置callback函数、上下文，并设置在scoped_fdref中，
     // 这里需要注意，fd发生事件改变(可读、可写、或都有)时，CFRunLoop会回调
     // HandleFdIOEvent()
    base::ScopedCFTypeRef<CFFileDescriptorRef> scoped_fdref(
        CFFileDescriptorCreate(kCFAllocatorDefault,
                               fd, false, HandleFdIOEvent,
                               &source_context));
    if (scoped_fdref == NULL) {
      NOTREACHED() << "CFFileDescriptorCreate failed";
      return false;
    }
    // 开启 fd 对应的回调，并设置回调的类型是可读，或可写，或全部
    CFFileDescriptorEnableCallBacks(scoped_fdref, callback_types);

    // TODO(wtc): what should the 'order' argument be?
    // 为 fd(scoped_fdref) 创建一个新的 runloop源
    base::ScopedCFTypeRef<CFRunLoopSourceRef> scoped_fd_source(
        CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault,
                                            scoped_fdref, 0));
    if (scoped_fd_source == NULL) {
      NOTREACHED() << "CFFileDescriptorCreateRunLoopSource failed";
      return false;
    }

    // 添加 scoped_fd_source(CFRunLoopSource) 到 CFRuntime
    // 其中 run_loop()返回 CFRunLoopRef
    CFRunLoopAddSource(run_loop(), scoped_fd_source, kCFRunLoopCommonModes);

    // Transfer ownership of scoped_fdref and fd_source to controller.
    // 将 scoped_fdref(fd，文件描述符) 和 fd_source 的所有权转移给控制器。
    controller->Init(scoped_fdref.release(), callback_types,
                     scoped_fd_source.release(), persistent);
  } else {
    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|.
    // 使用这个函数来监听 2 个具有相同 |controller| 的独立 fds 是非法的。
    if (CFFileDescriptorGetNativeDescriptor(fdref) != fd) {
      NOTREACHED() << "FDs don't match: "
                   << CFFileDescriptorGetNativeDescriptor(fdref)
                   << " != " << fd;
      return false;
    }
    if (persistent != controller->is_persistent_) {
      NOTREACHED() << "persistent doesn't match";
      return false;
    }

    // Combine old/new event masks.
    CFFileDescriptorDisableCallBacks(fdref, controller->callback_types_);
    controller->callback_types_ |= callback_types;
    // 开启 fd 对应的回调，并设置回调的类型是可读，或可写，或全部
    CFFileDescriptorEnableCallBacks(fdref, controller->callback_types_);
  }

  controller->set_watcher(delegate);
  controller->set_pump(weak_factory_.GetWeakPtr());

  return true;
}

void MessagePumpIOSForIO::RemoveRunLoopSource(CFRunLoopSourceRef source) {
  // 从 CFRunttime 中删除fd源(CFRunLoopSourceRef)，这样就没有callback了
  CFRunLoopRemoveSource(run_loop(), source, kCFRunLoopCommonModes);
}

/**
 * @brief CFRunLoop监听fd的callback
 */
// static
void MessagePumpIOSForIO::HandleFdIOEvent(CFFileDescriptorRef fdref,
                                          CFOptionFlags callback_types,
                                          void* context) {
  FdWatchController* controller = static_cast<FdWatchController*>(context);
  DCHECK_EQ(fdref, controller->fdref_.get());

  // Ensure that |fdref| will remain live for the duration of this function
  // call even if |controller| is deleted or |StopWatchingFileDescriptor()| is
  // called, either of which will cause |fdref| to be released.
  // 确保 |fdref| 将在此函数调用期间保持活动状态，即使 |controller| 被删除
  // 或 |StopWatchingFileDescriptor()| 被调用。
  // 作用域对象将保留fdref实例，并且任何初始所有权都不会更改。
  ScopedCFTypeRef<CFFileDescriptorRef> scoped_fdref(
      fdref, base::scoped_policy::RETAIN);

  // 获取真实的（裸）fd
  int fd = CFFileDescriptorGetNativeDescriptor(fdref);
  MessagePumpIOSForIO* pump = controller->pump().get();
  DCHECK(pump);
  // 可写callback类型，则回调可写
  if (callback_types & kCFFileDescriptorWriteCallBack)
    controller->OnFileCanWriteWithoutBlocking(fd, pump);

  // Perform the read callback only if the file descriptor has not been
  // invalidated in the write callback. As |FdWatchController| invalidates
  // its file descriptor on destruction, the file descriptor being valid also
  // guarantees that |controller| has not been deleted.
  // 仅当fd在写回调中未失效时才执行读回调。作为 |FdWatchController| 在销毁时使其
  // fd无效，有效的fd也保证 |controller| 没有被删除。
  // 可读callback类型，则回调可读
  if (callback_types & kCFFileDescriptorReadCallBack &&
      CFFileDescriptorIsValid(fdref)) {
    DCHECK_EQ(fdref, controller->fdref_.get());
    controller->OnFileCanReadWithoutBlocking(fd, pump);
  }

  // Re-enable callbacks after the read/write if the file descriptor is still
  // valid and the controller is persistent.
  // 如果fd仍然有效并且controller是持久的，则在读/写之后重新启用回调。
  if (CFFileDescriptorIsValid(fdref) && controller->is_persistent_) {
    DCHECK_EQ(fdref, controller->fdref_.get());
    // 重新启用回调
    CFFileDescriptorEnableCallBacks(fdref, callback_types);
  }
}

}  // namespace base
