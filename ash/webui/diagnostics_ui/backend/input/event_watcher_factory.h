// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_EVENT_WATCHER_FACTORY_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_EVENT_WATCHER_FACTORY_H_

#include <linux/input.h>
#include <cstdint>
#include <memory>

#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"
#include "base/memory/weak_ptr.h"

namespace ash::diagnostics {

// `EventWatcherFactory` interface for creating `InputDataEventWatcher` for
// different device types.
class EventWatcherFactory {
 public:
  virtual ~EventWatcherFactory();

  // Creates an `InputDataEventWatcher` which dispatches keyboard events.
  virtual std::unique_ptr<InputDataEventWatcher> MakeKeyboardEventWatcher(
      uint32_t evdev_id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher) = 0;

 protected:
  // Helper function avoid repeat code in Make<T>Watcher functions.
  // `EventWatcherImpl` inherit from InputDataEventWatcher.
  // `EventDispatcher` should provide an interface for dispatching events. See
  // `KeyboardInputDataEventWatcher::Dispatcher` for an example implementation.
  template <class EventWatcherImpl, class EventDispatcher>
  std::unique_ptr<EventWatcherImpl> MakeWatcher(
      uint32_t evdev_id,
      base::WeakPtr<EventDispatcher> dispatcher) {
    return std::make_unique<EventWatcherImpl>(evdev_id, std::move(dispatcher));
  }
};

// Implementation of `EventWatcherFactory`.
class EventWatcherFactoryImpl : public EventWatcherFactory {
 public:
  EventWatcherFactoryImpl() = default;
  EventWatcherFactoryImpl(const EventWatcherFactoryImpl&) = delete;
  EventWatcherFactoryImpl& operator=(const EventWatcherFactoryImpl&) = delete;
  ~EventWatcherFactoryImpl() override = default;

  std::unique_ptr<InputDataEventWatcher> MakeKeyboardEventWatcher(
      uint32_t evdev_id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
      override;
};

}  // namespace ash::diagnostics

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_INPUT_EVENT_WATCHER_FACTORY_H_
