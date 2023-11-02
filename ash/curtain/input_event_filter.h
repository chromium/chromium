// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_INPUT_EVENT_FILTER_H_
#define ASH_CURTAIN_INPUT_EVENT_FILTER_H_

#include "ash/curtain/security_curtain_controller.h"
#include "base/memory/raw_ref.h"
#include "ui/events/event_rewriter.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash::curtain {

// This class will observe all events in the given root window, and filter out
// the input events (keyboard/mouse/touch/...) that are rejected by the given
// |EventFilter|.
class InputEventFilter : public ui::EventRewriter {
 public:
  InputEventFilter(aura::Window* root_window, EventFilter filter);
  ~InputEventFilter() override;

 private:
  // ui::EventRewriter implementation:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  raw_ref<aura::Window> root_window_;
  EventFilter filter_;
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_INPUT_EVENT_FILTER_H_
