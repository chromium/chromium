// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_EVENT_WATCHER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_EVENT_WATCHER_H_

#include <linux/input.h>

#include <cstdint>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_pump_for_ui.h"

namespace ash::diagnostics {

// Interfaces for watching and dispatching relevant events from evdev to the
// input_data_provider.
class InputDataEventWatcher : public base::MessagePumpForUI::FdWatcher {
 public:
  // Constructor for unittests.
  InputDataEventWatcher(uint32_t evdev_id, const base::FilePath& path, int fd);
  explicit InputDataEventWatcher(uint32_t evdev_id);
  ~InputDataEventWatcher() override;

  // Interpret raw `input_event` components into logical events based on the
  // event protocols and connected device. Described by kernel input event-codes
  // api. See: https://www.kernel.org/doc/Documentation/input/event-codes.txt
  // for a list of valid codes.
  virtual void ProcessEvent(const input_event& event) = 0;

  void Start();
  void Stop();

 protected:
  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // Start watching file descriptor on current UI thread.
  virtual void DoStart();
  // Stop watching file descriptor on current UI thread.
  virtual void DoStop();
  virtual void DoOnFileCanReadWithoutBlocking(int fd);
  virtual void DoOnFileCanWriteWithoutBlocking(int fd) {}

  const uint32_t evdev_id_;    // input device evdev id.
  const base::FilePath path_;  // evdev path /dev/input/event{evdev_id_}.
  const int fd_;               // File descriptor being watched.
  const base::ScopedFD input_device_fd_;  // base::ScopedFD to ensure closed.
  base::MessagePumpForUI::FdWatchController controller_;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_INPUT_DATA_EVENT_WATCHER_H_
