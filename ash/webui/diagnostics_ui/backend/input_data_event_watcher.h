// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_

#include <linux/input.h>
#include <cstdint>

namespace ash::diagnostics {

// Interfaces for watching and dispatching relevant events from evdev to the
// input_data_provider.
class InputDataEventWatcher {
 public:
  virtual ~InputDataEventWatcher() = 0;

  // Interpret raw `input_event` components into logical events based on the
  // event protocols and connected device. Described by kernel input event-codes
  // api. See: https://www.kernel.org/doc/Documentation/input/event-codes.txt
  // for a list of valid codes.
  virtual void ProcessEvent(const input_event& event) = 0;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_
