// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
#define ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class ToastManagerImplTest;
class ToastOverlayView;
class ToastOverlayButton;

class ASH_EXPORT ToastOverlay : public ui::ImplicitAnimationObserver,
                                public KeyboardControllerObserver {
 public:
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnClosed() = 0;
  };

  // Offset of the overlay from the edge of the work area.
  static constexpr int kOffset = 16;

  // Creates the Toast overlay UI. |text| is the message to be shown, and
  // |dismiss_text| is the message for the button to dismiss the toast message.
  // If |dismiss_text| is null, no dismiss button will be shown. If
  // |dismiss_text| has a value but the string is empty, the default text is
  // used.
  ToastOverlay(Delegate* delegate,
               const base::string16& text,
               base::Optional<base::string16> dismiss_text,
               bool show_on_lock_screen = false);
  ~ToastOverlay() override;

  // Shows or hides the overlay.
  void Show(bool visible);

  // Update the position and size of toast.
  void UpdateOverlayBounds();

 private:
  friend class ToastManagerImplTest;

  class ToastDisplayObserver;

  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override;
  void OnImplicitAnimationsCompleted() override;

  // KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override;

  views::Widget* widget_for_testing();
  ToastOverlayButton* dismiss_button_for_testing();
  void ClickDismissButtonForTesting(const ui::Event& event);

  Delegate* const delegate_;
  const base::string16 text_;
  const base::Optional<base::string16> dismiss_text_;
  std::unique_ptr<views::Widget> overlay_widget_;
  std::unique_ptr<ToastOverlayView> overlay_view_;
  std::unique_ptr<ToastDisplayObserver> display_observer_;

  gfx::Size widget_size_;

  DISALLOW_COPY_AND_ASSIGN(ToastOverlay);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
