// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
#define ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "base/callback.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size.h"

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
  };

  // Offset of the overlay from the edge of the work area.
  static constexpr int kOffset = 16;

  // Creates the Toast overlay UI. `text` is the message to be shown, and
  // `dismiss_text` is the message for the button to dismiss the toast message.
  // The dismiss button will only be displayed if `dismiss_text` is not empty.
  // `dismiss_callback` will be called when the button is pressed.
  // `expired_callback` will be called when the toast overlay is destroyed,
  // regardless of whether the button was pressed. In other words,
  // `expired_callback` is called whenever the toast disappears. If `is_managed`
  // is true, a managed icon will be added to the toast.
  ToastOverlay(Delegate* delegate,
               const std::u16string& text,
               const std::u16string& dismiss_text,
               bool show_on_lock_screen,
               bool is_managed,
               base::RepeatingClosure dismiss_callback,
               base::RepeatingClosure expired_callback);

  ToastOverlay(const ToastOverlay&) = delete;
  ToastOverlay& operator=(const ToastOverlay&) = delete;

  ~ToastOverlay() override;

  // Shows or hides the overlay.
  void Show(bool visible);

  // Update the position and size of toast.
  void UpdateOverlayBounds();

  const std::u16string GetText();

 private:
  friend class ToastManagerImplTest;
  friend class DesksTestApi;

  class ToastDisplayObserver;

  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

  // Executed the callback and closes the toast.
  void OnButtonClicked();

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
  base::RepeatingClosure dismiss_callback_;
  base::RepeatingClosure expired_callback_;

  gfx::Size widget_size_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_TOAST_OVERLAY_H_
