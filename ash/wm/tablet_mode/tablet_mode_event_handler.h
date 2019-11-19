// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_EVENT_HANDLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_EVENT_HANDLER_H_

#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ui {
class TouchEvent;
}

namespace ash {

// TabletModeEventHandler handles toggling fullscreen when appropriate.
// TabletModeEventHandler installs event handlers in an environment specific
// way, e.g. EventHandler for aura.
class TabletModeEventHandler : public ui::EventHandler {
 public:
  TabletModeEventHandler();
  ~TabletModeEventHandler() override;

 private:
  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override;

  // Returns true if a toggle happened.
  bool ToggleFullscreen(const ui::TouchEvent& event);

  DISALLOW_COPY_AND_ASSIGN(TabletModeEventHandler);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_EVENT_HANDLER_H_
