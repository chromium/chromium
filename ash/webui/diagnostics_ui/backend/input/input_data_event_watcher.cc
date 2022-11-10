// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/input_data_event_watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"

namespace ash::diagnostics {

namespace {

constexpr int kOpenFlags = O_NONBLOCK | O_RDWR;

}

InputDataEventWatcher::InputDataEventWatcher(uint32_t evdev_id,
                                             const base::FilePath& path,
                                             int fd)
    : evdev_id_(evdev_id),
      path_(path),
      fd_(fd),
      input_device_fd_(fd_),
      controller_(FROM_HERE) {}

InputDataEventWatcher::InputDataEventWatcher(uint32_t evdev_id)
    : evdev_id_(evdev_id),
      path_(base::StringPrintf("/dev/input/event%d", evdev_id_)),
      fd_(open(path_.value().c_str(), kOpenFlags)),
      input_device_fd_(fd_),
      controller_(FROM_HERE) {
  if (fd_ == -1) {
    PLOG(ERROR) << "Unable to open event device " << evdev_id_
                << ", not forwarding events for input diagnostics.";
    // Leave un-Started(), so we never enable the fd watcher.
    return;
  }

  Start();
}

InputDataEventWatcher::~InputDataEventWatcher() {
  controller_.StopWatchingFileDescriptor();
}

void InputDataEventWatcher::Start() {
  DoStart();
}

void InputDataEventWatcher::Stop() {
  DoStop();
}

void InputDataEventWatcher::OnFileCanReadWithoutBlocking(int fd) {
  DoOnFileCanReadWithoutBlocking(fd);
}

void InputDataEventWatcher::OnFileCanWriteWithoutBlocking(int fd) {
  DoOnFileCanWriteWithoutBlocking(fd);
}

void InputDataEventWatcher::DoStart() {
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd_, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
}

void InputDataEventWatcher::DoStop() {
  controller_.StopWatchingFileDescriptor();
}

void InputDataEventWatcher::DoOnFileCanReadWithoutBlocking(int fd) {
  while (true) {
    input_event input;
    ssize_t read_size = read(fd, &input, sizeof(input));
    // Read of input_event from `fd` completed with unexpected number of bytes.
    if (read_size != sizeof(input)) {
      // Recoverable failure. Wait for FDWatcher to trigger
      // `OnFileCanReadWithoutBlocking` again. EINTR occurs when read is
      // interrupted before it can complete. EAGAIN occurs when non-blocking
      // socket read would block.
      if (errno == EINTR || errno == EAGAIN)
        return;
      // Not recoverable failure. Stop watching file descriptor.
      if (errno != ENODEV)
        PLOG(ERROR) << "error reading device " << path_.value();
      Stop();
      return;
    }

    ProcessEvent(input);
  }
}

}  // namespace ash::diagnostics
