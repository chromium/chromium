// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
#define ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/size.h"

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

class ToastManagerImplTest;
class SystemToastStyle;

class ASH_EXPORT ToastOverlay : public ui::ImplicitAnimationObserver,
                                public KeyboardControllerObserver {
 public:
  class ASH_EXPORT Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnClosed() = 0;

    // Called when a toast's hover state changed if the toast is supposed to
    // persist on hover.
    virtual void OnToastHoverStateChanged(bool is_hovering) = 0;
  };

  // Offset of the overlay from the edge of the work area.
  static constexpr int kOffset = 16;

  // Creates the Toast overlay UI. `text` is the message to be shown, and
  // `dismiss_text` is the message for the button to dismiss the toast message.
  // The dismiss button will only be displayed if `dismiss_text` is not empty.
  // `dismiss_callback` will be called when the button is pressed. If
  // `is_managed` is true, a managed icon will be added to the toast.
  ToastOverlay(Delegate* delegate,
               const std::u16string& text,
               const std::u16string& dismiss_text,
               base::TimeDelta duration,
               bool show_on_lock_screen,
               bool is_managed,
               bool persist_on_hover,
               aura::Window* root_window,
               base::RepeatingClosure dismiss_callback);

  ToastOverlay(const ToastOverlay&) = delete;
  ToastOverlay& operator=(const ToastOverlay&) = delete;

  ~ToastOverlay() override;

  // Shows or hides the overlay.
  void Show(bool visible);

  // Update the position and size of toast.
  void UpdateOverlayBounds();

  const std::u16string GetText();

  // Returns true if the toast has a button and it can be highlighted for
  // accessibility, false otherwise.
  bool MaybeToggleA11yHighlightOnDismissButton();

  // Activates the dismiss button in `overlay_view_` if it is highlighted.
  // Returns false if `is_dismiss_button_highlighted_` is false.
  bool MaybeActivateHighlightedDismissButton();

 private:
  friend class ToastManagerImplTest;
  friend class ClipboardHistoryControllerRefreshTest;
  friend class DesksTestApi;

  class ToastDisplayObserver;
  class ToastHoverObserver;

  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

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

  views::Widget* widget_for_testing();
  views::LabelButton* dismiss_button_for_testing();

  Delegate* const delegate_;
  const std::u16string text_;
  const std::u16string dismiss_text_;
  std::unique_ptr<views::Widget> overlay_widget_;
  std::unique_ptr<SystemToastStyle> overlay_view_;
  std::unique_ptr<ToastDisplayObserver> display_observer_;
  aura::Window* root_window_;
  base::RepeatingClosure dismiss_callback_;

  gfx::Size widget_size_;

  // Used to pause and resume the `ToastManagerImpl`'s
  // `current_toast_expiration_timer_` if we are allowing for the toast to
  // persist on hover.
  std::unique_ptr<ToastHoverObserver> hover_observer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
