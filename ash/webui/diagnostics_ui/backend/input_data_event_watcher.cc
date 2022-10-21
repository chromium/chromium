// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_event_watcher.h"

#include <fcntl.h>
#include <linux/input.h>
#include <cstdint>
#include <memory>

#include "ash/webui/diagnostics_ui/backend/keyboard_input_data_event_watcher.h"

namespace ash::diagnostics {

InputDataEventWatcher::~InputDataEventWatcher() = default;
InputDataEventWatcher::Factory::~Factory() = default;

std::unique_ptr<InputDataEventWatcher>
InputDataEventWatcherFactoryImpl::MakeWatcher(
    uint32_t id,
    base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher) {
  return std::make_unique<KeyboardInputDataEventWatcher>(id,
                                                         std::move(dispatcher));
}

}  // namespace ash::diagnostics
