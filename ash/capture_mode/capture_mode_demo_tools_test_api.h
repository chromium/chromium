// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"

namespace views {
class ImageView;
class Widget;
}  // namespace views

namespace ash {

class CaptureModeDemoToolsController;
class KeyComboView;
class PointerHighlightLayer;

using MouseHighlightLayers =
    std::vector<std::unique_ptr<PointerHighlightLayer>>;

using TouchHighlightLayersMap =
    base::flat_map<ui::PointerId, std::unique_ptr<PointerHighlightLayer>>;

class CaptureModeDemoToolsTestApi {
 public:
  explicit CaptureModeDemoToolsTestApi(
      CaptureModeDemoToolsController* demo_tools_controller);
  CaptureModeDemoToolsTestApi(CaptureModeDemoToolsTestApi&) = delete;
  CaptureModeDemoToolsTestApi& operator=(CaptureModeDemoToolsTestApi) = delete;
  ~CaptureModeDemoToolsTestApi() = default;

  views::Widget* GetKeyComboWidget();

  // Returns the contents view for the `key_combo_widget_`.
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

  // Returns the timer to hide the key combo viewer on key up of the
  // non-modifier key after the expiration.
  base::OneShotTimer* GetRefreshKeyComboTimer();

  // Returns the `icon_` of the non-modifier component of the key combo.
  views::ImageView* GetNonModifierKeyItemIcon();

  // Sets a callback that will be triggered once the mouse highlight animation
  // ends.
  void SetOnMouseHighlightAnimationEndedCallback(base::OnceClosure callback);

  const MouseHighlightLayers& GetMouseHighlightLayers() const;

  const TouchHighlightLayersMap& GetTouchIdToHighlightLayerMap() const;

 private:
  const raw_ptr<CaptureModeDemoToolsController, DanglingUntriaged>
      demo_tools_controller_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_DEMO_TOOLS_TEST_API_H_
