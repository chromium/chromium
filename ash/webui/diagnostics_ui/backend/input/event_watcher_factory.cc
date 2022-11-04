// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/event_watcher_factory.h"

#include <cstdint>
#include <memory>

#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"
#include "base/memory/weak_ptr.h"

namespace ash::diagnostics {

EventWatcherFactory::~EventWatcherFactory() = default;

std::unique_ptr<InputDataEventWatcher>
EventWatcherFactoryImpl::MakeKeyboardEventWatcher(
    uint32_t evdev_id,
    base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher) {
  return this->MakeWatcher<KeyboardInputDataEventWatcher,
                           KeyboardInputDataEventWatcher::Dispatcher>(
      evdev_id, dispatcher);
}

}  // namespace ash::diagnostics
