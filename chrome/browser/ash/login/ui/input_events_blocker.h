// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_INPUT_EVENTS_BLOCKER_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_INPUT_EVENTS_BLOCKER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace chromeos {

// A simple input events blocker that just makes device unresponsive.
// Should be used only for a (very) short time lock as it's confusing to the
// user.
class InputEventsBlocker : public ui::EventHandler {
 public:
  InputEventsBlocker();
  ~InputEventsBlocker() override;

  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputEventsBlocker);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_INPUT_EVENTS_BLOCKER_H_
