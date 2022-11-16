// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_BUTTON_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

// ActionEditButton is menu entry for editing each action.
class ActionEditButton : public views::ImageButton {
 public:
  explicit ActionEditButton(PressedCallback callback = PressedCallback());
  ActionEditButton(const ActionEditButton&) = delete;
  ActionEditButton& operator=(const ActionEditButton&) = delete;
  ~ActionEditButton() override;

 private:
  class CircleBackground;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_BUTTON_H_
