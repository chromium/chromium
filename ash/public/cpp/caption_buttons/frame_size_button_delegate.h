// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/caption_buttons/caption_button_types.h"
#include "ash/public/cpp/caption_buttons/frame_caption_delegate.h"

namespace gfx {
class Point;
}

namespace ash {
class FrameCaptionButton;

// Delegate interface for FrameSizeButton.
class ASH_PUBLIC_EXPORT FrameSizeButtonDelegate {
 public:
  enum Animate { ANIMATE_YES, ANIMATE_NO };

  // Returns whether the minimize button is visible.
  virtual bool IsMinimizeButtonVisible() const = 0;

  // Reset the caption button views::Button::ButtonState back to normal. If
  // |animate| is ANIMATE_YES, the buttons will crossfade back to their
  // original icons.
  virtual void SetButtonsToNormal(Animate animate) = 0;

  // Sets the minimize and close button icons. The buttons will crossfade to
  // their new icons if |animate| is ANIMATE_YES.
  virtual void SetButtonIcons(CaptionButtonIcon minimize_button_icon,
                              CaptionButtonIcon close_button_icon,
                              Animate animate) = 0;

  // Returns the button closest to |position_in_screen|.
  virtual const FrameCaptionButton* GetButtonClosestTo(
      const gfx::Point& position_in_screen) const = 0;

  // Sets |to_hover| and |to_pressed| to STATE_HOVERED and STATE_PRESSED
  // respectively. All other buttons are to set to STATE_NORMAL.
  virtual void SetHoveredAndPressedButtons(
      const FrameCaptionButton* to_hover,
      const FrameCaptionButton* to_press) = 0;

  // Thunks to methods of the same name in FrameCaptionDelegate.
  virtual bool CanSnap() = 0;
  virtual void ShowSnapPreview(mojom::SnapDirection snap) = 0;
  virtual void CommitSnap(mojom::SnapDirection snap) = 0;

 protected:
  virtual ~FrameSizeButtonDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_SIZE_BUTTON_DELEGATE_H_
