// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_KEYBOARD_INPUT_DATA_EVENT_WATCHER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_KEYBOARD_INPUT_DATA_EVENT_WATCHER_H_

#include "ash/webui/diagnostics_ui/backend/input/input_data_event_watcher.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

namespace ash::diagnostics {

// Class for dispatching relevant events from evdev to the input_data_provider.
// While it would be nice to re-use EventConverterEvdevImpl for this purpose,
// it has a lot of connections (ui::Cursor, full ui::DeviceEventDispatcherEvdev
// interface) that take more room to stub out rather than just implementing
// another evdev FdWatcher from scratch.
class KeyboardInputDataEventWatcher : public InputDataEventWatcher {
 public:
  // `KeyboardInputDataEventWatcher` calls dispatcher when a key event is ready.
  class Dispatcher {
   public:
    virtual ~Dispatcher() = default;

    // Emits the components of a key event.
    //  `id` maps to the evdev id.
    //  `key_code` defined in <linux/input-event-codes.h>.`
    //  `scan_code` additional key idetification data.
    //  `down` whether key is identified is currently pressed.
    virtual void SendInputKeyEvent(uint32_t id,
                                   uint32_t key_code,
                                   uint32_t scan_code,
                                   bool down) = 0;
  };

  KeyboardInputDataEventWatcher(
      uint32_t evdev_id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher);

  // Constructor for unittests.
  KeyboardInputDataEventWatcher(
      uint32_t evdev_id,
      const base::FilePath& path,
      int fd,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher);

  ~KeyboardInputDataEventWatcher() override;

  void ConvertKeyEvent(uint32_t key_code,
                       uint32_t key_state,
                       uint32_t scan_code);

  // InputDataEventWatcher:
  void ProcessEvent(const input_event& input) override;

 protected:
  // EV_ information pending for SYN_REPORT to dispatch.
  uint32_t pending_scan_code_ = 0;
  uint32_t pending_key_code_ = 0;
  uint32_t pending_key_state_ = 0;

  base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher_;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_KEYBOARD_INPUT_DATA_EVENT_WATCHER_H_
