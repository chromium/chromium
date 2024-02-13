// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class LabelButton;
class Separator;
}  // namespace views

namespace ash {

class CaptureModeBehavior;

// Defines a view that will host the capture button which when pressed, the
// screen capture operation will be performed. In the case of video recording,
// if multiple recording formats are supported, it will display a drop down
// button which when pressed will open the recording type selection menu.
class CaptureButtonView : public views::View {
  METADATA_HEADER(CaptureButtonView, views::View)

 public:
  CaptureButtonView(views::Button::PressedCallback on_capture_button_pressed,
                    views::Button::PressedCallback on_drop_down_pressed,
                    CaptureModeBehavior* active_behavior);
  CaptureButtonView(const CaptureButtonView&) = delete;
  CaptureButtonView& operator=(const CaptureButtonView&) = delete;
  ~CaptureButtonView() override = default;

  views::LabelButton* capture_button() { return capture_button_; }
  views::ImageButton* drop_down_button() { return drop_down_button_; }

  // Updates the icon and text of `capture_button_`, as well as the visibility
  // of the `separator_` and `drop_down_button_` depending on the current type
  // of capture. It also updates the shape of the focus ring of the
  // `capture_button_` as it can switch from being fully rounded to half rounded
  // when the visibility of the separator changes. This should only be called
  // when this view is visible.
  void UpdateViewVisuals();

  // Returns the list of avaibale buttons that can be highlighted while
  // navigating with keyboard.
  std::vector<CaptureModeSessionFocusCycler::HighlightableView*>
  GetHighlightableItems() const;

  // views::View:
  void OnThemeChanged() override;

 private:
  // Sets up the given `button`'s ink drop style and focus behavior.
  void SetupButton(views::Button* button);

  // Bound to callbacks that will create a path generator for both the capture
  // and the drop down buttons. If `use_zero_insets` is true, no insets will be
  // added to the resulting path generator. This is useful when using the path
  // generator for the ink drop highlight which should not have any insets,
  // unlike the focus ring which should be insetted a little to be drawn within
  // the bounds of the view.
  std::unique_ptr<views::HighlightPathGenerator> CreateFocusRingPath(
      views::View* view,
      bool use_zero_insets);

  // The button which when pressed, screen capture will be performed.
  const raw_ptr<views::LabelButton> capture_button_;

  // Optional views that are created only, when multiple (i.e. more than one)
  // recording formats (e.g. webm, gif, .. etc.) are supported. They're visible
  // only if the current capture type is video recording.
  raw_ptr<views::Separator> separator_ = nullptr;
  raw_ptr<views::ImageButton> drop_down_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_
