// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/keyboard_input_data_event_watcher.h"

#include <fcntl.h>
#include <linux/input.h>
#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"

namespace ash::diagnostics {

namespace {

const int kKeyReleaseValue = 0;

}  // namespace

KeyboardInputDataEventWatcher::KeyboardInputDataEventWatcher(
    uint32_t id,
    base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
    : id_(id),
      path_(base::FilePath(base::StringPrintf("/dev/input/event%d", id_))),
      fd_(open(path_.value().c_str(), O_RDWR | O_NONBLOCK)),
      input_device_fd_(fd_),
      dispatcher_(dispatcher),
      controller_(FROM_HERE) {
  if (fd_ == -1) {
    PLOG(ERROR) << "Unable to open event device " << id_
                << ", not forwarding events for input diagnostics.";
    // Leave un-Started(), so we never enable the fd watcher.
    return;
  }

  Start();
}

KeyboardInputDataEventWatcher::KeyboardInputDataEventWatcher(
    uint32_t id,
    const base::FilePath& device_path,
    int read_fd,
    base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
    : id_(id),
      path_(device_path),
      fd_(read_fd),
      input_device_fd_(fd_),
      dispatcher_(dispatcher),
      controller_(FROM_HERE) {}

KeyboardInputDataEventWatcher::~KeyboardInputDataEventWatcher() = default;

void KeyboardInputDataEventWatcher::Start() {
  base::CurrentUIThread::Get()->WatchFileDescriptor(
      fd_, true, base::MessagePumpForUI::WATCH_READ, &controller_, this);
  watching_ = true;
}

void KeyboardInputDataEventWatcher::Stop() {
  controller_.StopWatchingFileDescriptor();
  watching_ = false;
}

void KeyboardInputDataEventWatcher::SetQuitClosureForTesting(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

void KeyboardInputDataEventWatcher::OnFileCanReadWithoutBlocking(int fd) {
  while (true) {
    input_event input;
    ssize_t read_size = read(fd, &input, sizeof(input));
    if (read_size != sizeof(input)) {
      if (errno == EINTR || errno == EAGAIN)
        return;
      if (errno != ENODEV)
        PLOG(ERROR) << "error reading device " << path_.value();
      Stop();
      return;
    }

    ProcessEvent(input);
  }
}

void KeyboardInputDataEventWatcher::OnFileCanWriteWithoutBlocking(int fd) {}

// Once we have an entire keypress/release, dispatch it.
void KeyboardInputDataEventWatcher::ConvertKeyEvent(uint32_t key_code,
                                                    uint32_t key_state,
                                                    uint32_t scan_code) {
  bool down = key_state != kKeyReleaseValue;
  if (dispatcher_)
    dispatcher_->SendInputKeyEvent(id_, key_code, scan_code, down);

  // `quit_closure_` used to stop tests.
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

// Process evdev event structures directly from the kernel.
void KeyboardInputDataEventWatcher::ProcessEvent(const input_event& input) {
  // Accumulate relevant data about an event until a SYN_REPORT event releases
  // the full report. For more information, see kernel documentation for
  // input/event-codes.rst.
  switch (input.type) {
    case EV_MSC:
      if (input.code == MSC_SCAN)
        pending_scan_code_ = input.value;
      break;
    case EV_KEY:
      pending_key_code_ = input.code;
      pending_key_state_ = input.value;
      break;
    case EV_SYN:
      if (input.code == SYN_REPORT)
        ConvertKeyEvent(pending_key_code_, pending_key_state_,
                        pending_scan_code_);
      pending_key_code_ = 0;
      pending_key_state_ = 0;
      pending_scan_code_ = 0;
      break;
  }
}

}  // namespace ash::diagnostics
