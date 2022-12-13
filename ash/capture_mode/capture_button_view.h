// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class LabelButton;
class Separator;
}  // namespace views

namespace ash {

// Defines a view that will host the capture button which when pressed, the
// screen capture operation will be performed. In the case of video recording,
// if multiple recording formats are supported, it will display a drop down
// button which when pressed will open the recording type selection menu.
class CaptureButtonView : public views::View {
 public:
  METADATA_HEADER(CaptureButtonView);

  CaptureButtonView(base::RepeatingClosure on_capture_button_pressed,
                    base::RepeatingClosure on_drop_down_pressed);
  CaptureButtonView(const CaptureButtonView&) = delete;
  CaptureButtonView& operator=(const CaptureButtonView&) = delete;
  ~CaptureButtonView() override = default;

  views::LabelButton* capture_button() { return capture_button_; }
  views::ImageButton* drop_down_button() { return drop_down_button_; }

  // Updates the icon and text of `capture_button_`, as well as the visibility
  // of the `separator_` and `drop_down_button_` depending on the current type
  // of capture. This should only be called when this view is visible.
  void UpdateViewVisuals();

  // views::View:
  void OnThemeChanged() override;

 private:
  // The button which when pressed, screen capture will be performed.
  views::LabelButton* const capture_button_;

  // Optional views that are created only, when multiple recording formats (e.g.
  // webm, gif, .. etc.) are supported. They're visible only if the current
  // capture type is video recording.
  views::Separator* separator_ = nullptr;
  views::ImageButton* drop_down_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_BUTTON_VIEW_H_
