// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
#define ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class LabelButton;
class Widget;
}

namespace ash {

class SystemToastView;
class ToastManagerImplTest;

class ASH_EXPORT ToastOverlay : public ui::ImplicitAnimationObserver,
                                public KeyboardControllerObserver,
                                public ShelfObserver,
                                public UnifiedSystemTray::Observer {
 public:
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}
    virtual void CloseToast() = 0;

    // Called when a toast's hover state changed if the toast is supposed to
    // persist on hover.
    virtual void OnToastHoverStateChanged(bool is_hovering) = 0;
  };

  // Offset of the overlay from the edge of the work area.
  static constexpr int kOffset = 8;

  // Creates the Toast overlay UI. `text` is the message to be shown, and
  // `dismiss_text` is the dismiss button's text. The dismiss button will only
  // be displayed if `dismiss_text` is not empty. `dismiss_callback` will be
  // called when the dismiss button is pressed. An icon will show on the left
  // side if `leading_icon` is not empty.
  // To test different Toast UI variations, enable debug shortcuts by building
  // with flag `--ash-debug-shortcuts` and use command "Shift + Ctrl + Alt + O".
  ToastOverlay(Delegate* delegate,
               const ToastData& toast_data,
               aura::Window* root_window);

  ToastOverlay(const ToastOverlay&) = delete;
  ToastOverlay& operator=(const ToastOverlay&) = delete;

  ~ToastOverlay() override;

  // Shows or hides the overlay.
  void Show(bool visible);

  // Update the position and size of toast.
  void UpdateOverlayBounds();

  const std::u16string GetText() const;

  // Requests focus on the toast's dismiss button. Return true if it was
  // successful.
  bool RequestFocusOnActiveToastDismissButton();

  // Returns if the dismiss button is focused in the toast. If the toast does
  // not have a dismiss button, it returns false.
  bool IsDismissButtonFocused() const;

  // UnifiedSystemTray::Observer:
  void OnSliderBubbleHeightChanged() override;

 private:
  friend class ToastManagerImplTest;
  friend class DesksTestApi;

  class ToastDisplayObserver;
  class ToastHoverObserver;

  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

  // Calculates the y offset used to shift side aligned toasts up whenever a
  // slider bubble is visible.
  int CalculateSliderBubbleOffset();

  // Executed the callback and closes the toast.
  void OnButtonClicked();

  // Callback called by `hover_observer_` when the mouse hover enters or exits
  // the toast.
  void OnHoverStateChanged(bool is_hovering);

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override;
  void OnImplicitAnimationsCompleted() override;

  // KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override;

  // ShelfObserver:
  void OnShelfWorkAreaInsetsChanged() override;
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override;

  views::Widget* widget_for_testing();
  views::LabelButton* dismiss_button_for_testing();

  const raw_ptr<Delegate> delegate_;
  const std::u16string text_;
  const std::u16string dismiss_text_;
  std::unique_ptr<views::Widget> overlay_widget_;
  std::unique_ptr<SystemToastView> overlay_view_;
  std::unique_ptr<ToastDisplayObserver> display_observer_;
  raw_ptr<aura::Window> root_window_;
  base::RepeatingClosure dismiss_callback_;

  // Used to pause and resume the `ToastManagerImpl`'s
  // `current_toast_expiration_timer_` if we are allowing for the toast to
  // persist on hover.
  std::unique_ptr<ToastHoverObserver> hover_observer_;

  base::ScopedObservation<UnifiedSystemTray, UnifiedSystemTray::Observer>
      scoped_unified_system_tray_observer_{this};

  // Used to observe shelf and hotseat state to update toast baseline.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
