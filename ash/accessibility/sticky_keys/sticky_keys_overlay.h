// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_
#define ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_

#include <memory>

#include "ash/accessibility/sticky_keys/sticky_keys_state.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class StickyKeysOverlayView;

// Controls the overlay UI for sticky keys, an accessibility feature allowing
// use of modifier keys without holding them down. This overlay will appear as
// a transparent window on the top left of the screen, showing the state of
// each sticky key modifier.
class ASH_EXPORT StickyKeysOverlay : public ui::ImplicitAnimationObserver {
 public:
  StickyKeysOverlay();
  ~StickyKeysOverlay() override;

  // Shows or hides the overlay.
  void Show(bool visible);

  void UpdateBoundsIfVisible();

  void SetModifierVisible(ui::EventFlags modifier, bool visible);

  bool GetModifierVisible(ui::EventFlags modifier);

  // Updates the overlay with the current state of a sticky key modifier.
  void SetModifierKeyState(ui::EventFlags modifier, StickyKeyState state);

  // Get the current state of the sticky key modifier in the overlay.
  StickyKeyState GetModifierKeyState(ui::EventFlags modifier);

  // Returns true if the overlay is currently visible. If the overlay is
  // animating, the returned value is the target of the animation.
  bool is_visible() { return is_visible_; }

  // Returns the underlying views::Widget for testing purposes. The returned
  // widget is owned by StickyKeysOverlay.
  views::Widget* GetWidgetForTesting();

 private:
  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  bool is_visible_ = false;
  std::unique_ptr<views::Widget> overlay_widget_;
  raw_ptr<StickyKeysOverlayView> overlay_view_;  // owned by |overlay_widget_|.
  gfx::Size widget_size_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_
