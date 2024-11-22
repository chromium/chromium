// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/io_watcher.h"

#include <memory>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"

namespace base {

IOWatcher::IOWatcher() = default;

IOWatcher* IOWatcher::Get() {
  if (!CurrentThread::IsSet()) {
    return nullptr;
  }
  return CurrentThread::Get()->GetIOWatcher();
}

#if !BUILDFLAG(IS_NACL)
#if BUILDFLAG(IS_WIN)
bool IOWatcher::RegisterIOHandler(HANDLE file,
                                  MessagePumpForIO::IOHandler* handler) {
  return RegisterIOHandlerImpl(file, handler);
}

bool IOWatcher::RegisterJobObject(HANDLE job,
                                  MessagePumpForIO::IOHandler* handler) {
  return RegisterJobObjectImpl(job, handler);
}
#elif BUILDFLAG(IS_POSIX)
std::unique_ptr<IOWatcher::FdWatch> IOWatcher::WatchFileDescriptor(
    int fd,
    FdWatchDuration duration,
    FdWatchMode mode,
    FdWatcher& fd_watcher,
    const Location& location) {
  return WatchFileDescriptorImpl(fd, duration, mode, fd_watcher, location);
}
#endif

#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_IOS) && !BUILDFLAG(CRONET_BUILD))
bool IOWatcher::WatchMachReceivePort(
    mach_port_t port,
    MessagePumpForIO::MachPortWatchController* controller,
    MessagePumpForIO::MachPortWatcher* delegate) {
  return WatchMachReceivePortImpl(port, controller, delegate);
}
#elif BUILDFLAG(IS_FUCHSIA)
bool IOWatcher::WatchZxHandle(
    zx_handle_t handle,
    bool persistent,
    zx_signals_t signals,
    MessagePumpForIO::ZxHandleWatchController* controller,
    MessagePumpForIO::ZxHandleWatcher* delegate) {
  return WatchZxHandleImpl(handle, persistent, signals, controller, delegate);
}
#endif
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace base
