// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace views {
class ImageView;
class Widget;
}  // namespace views

namespace ash {

class CaptureModeDemoToolsController;
class KeyComboView;

class CaptureModeDemoToolsTestApi {
 public:
  explicit CaptureModeDemoToolsTestApi(
      CaptureModeDemoToolsController* demo_tools_controller);
  CaptureModeDemoToolsTestApi(CaptureModeDemoToolsTestApi&) = delete;
  CaptureModeDemoToolsTestApi& operator=(CaptureModeDemoToolsTestApi) = delete;
  ~CaptureModeDemoToolsTestApi() = default;

  views::Widget* GetDemoToolsWidget();

  // Returns the contents view for the `demo_tools_widget_`.
  KeyComboView* GetKeyComboView();

  // Returns the state of modifier keys in the `CaptureModeDemoToolsController`.
  int GetCurrentModifiersFlags();

  // Returns the most recently pressed non-modifier key in the
  // `CaptureModeDemoToolsController`.
  ui::KeyboardCode GetLastNonModifierKey();

  // Returns the key code vector in the `ModifiersContainerView` of the
  // `KeyComboView`.
  std::vector<ui::KeyboardCode> GetShownModifiersKeyCodes();

  // Returns the non-modifier key that is currently on display.
  ui::KeyboardCode GetShownNonModifierKeyCode();

  // Returns the timer to hide the key combo view on key up of the
  // non-modifier key after the expiration.
  base::OneShotTimer* GetKeyComboHideTimer();

  // Returns the `icon_` of the non-modifier component of the key combo.
  views::ImageView* GetNonModifierKeyItemIcon();

  // Sets a callback that will be triggered once the mouse highlight animation
  // ends.
  void SetOnMouseHighlightAnimationEndedCallback(base::OnceClosure callback);

 private:
  CaptureModeDemoToolsController* const demo_tools_controller_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_