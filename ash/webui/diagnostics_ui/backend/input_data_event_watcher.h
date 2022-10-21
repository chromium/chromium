// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_

#include <linux/input.h>
#include <cstdint>
#include <memory>

#include "base/memory/weak_ptr.h"

namespace ash::diagnostics {

// Interfaces for watching and dispatching relevant events from evdev to the
// input_data_provider.
class InputDataEventWatcher {
 public:
  class Dispatcher {
   public:
    virtual void SendInputKeyEvent(uint32_t id,
                                   uint32_t key_code,
                                   uint32_t scan_code,
                                   bool down) = 0;
  };

  class Factory {
   public:
    virtual ~Factory() = 0;

    virtual std::unique_ptr<InputDataEventWatcher> MakeWatcher(
        uint32_t evdev_id,
        base::WeakPtr<Dispatcher> dispatcher) = 0;
  };

  virtual ~InputDataEventWatcher() = 0;
};

class InputDataEventWatcherFactoryImpl : public InputDataEventWatcher::Factory {
 public:
  InputDataEventWatcherFactoryImpl() = default;
  InputDataEventWatcherFactoryImpl(const InputDataEventWatcherFactoryImpl&) =
      delete;
  InputDataEventWatcherFactoryImpl& operator=(
      const InputDataEventWatcherFactoryImpl&) = delete;
  ~InputDataEventWatcherFactoryImpl() override = default;

  std::unique_ptr<InputDataEventWatcher> MakeWatcher(
      uint32_t id,
      base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher) override;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_DATA_EVENT_WATCHER_H_
