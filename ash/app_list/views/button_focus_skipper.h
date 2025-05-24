// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_BUTTON_FOCUS_SKIPPER_H_
#define ASH_APP_LIST_VIEWS_BUTTON_FOCUS_SKIPPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"

namespace ui {
class Event;
class EventTarget;
}

namespace views {
class View;
}

namespace ash {

// Makes focus traversal skip the assistant button and the hide continue section
// button when pressing the down arrow key or the up arrow key. Normally views
// would move focus from the search box to the assistant button on arrow down.
// However, these buttons are visually to the right, so this feels weird.
// Likewise, on arrow up from continue tasks it feels better to put focus
// directly in the search box.
class ButtonFocusSkipper : public ui::EventHandler {
 public:
  explicit ButtonFocusSkipper(ui::EventTarget* event_target);

  ~ButtonFocusSkipper() override;

  void AddButton(views::View* button);

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override;

 private:
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons_;
  const raw_ptr<ui::EventTarget> event_target_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_BUTTON_FOCUS_SKIPPER_H_
