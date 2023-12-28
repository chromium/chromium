// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/button.h"

namespace arc {

class CompatModeButtonController;

class CompatModeButton : public chromeos::FrameCenterButton {
 public:
  CompatModeButton(CompatModeButtonController* controller,
                   PressedCallback callback);
  ~CompatModeButton() override = default;

  // chromeos::FrameCenterButton:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  const raw_ptr<CompatModeButtonController> controller_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_H_
