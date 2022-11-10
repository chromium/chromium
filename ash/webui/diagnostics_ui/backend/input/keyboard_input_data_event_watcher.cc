// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"

#include <fcntl.h>
#include <linux/input.h>

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"

namespace ash::diagnostics {

namespace {

const int kKeyReleaseValue = 0;

}  // namespace

KeyboardInputDataEventWatcher::KeyboardInputDataEventWatcher(
    uint32_t evdev_id,
    const base::FilePath& path,
    int fd,
    base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
    : InputDataEventWatcher(evdev_id, path, fd), dispatcher_(dispatcher) {}

KeyboardInputDataEventWatcher::KeyboardInputDataEventWatcher(
    uint32_t evdev_id,
    base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
    : InputDataEventWatcher(evdev_id), dispatcher_(dispatcher) {}

KeyboardInputDataEventWatcher::~KeyboardInputDataEventWatcher() = default;

// Once we have an entire keypress/release, dispatch it.
void KeyboardInputDataEventWatcher::ConvertKeyEvent(uint32_t key_code,
                                                    uint32_t key_state,
                                                    uint32_t scan_code) {
  bool down = key_state != kKeyReleaseValue;
  if (dispatcher_)
    dispatcher_->SendInputKeyEvent(this->evdev_id_, key_code, scan_code, down);
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
