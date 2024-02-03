// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class TabSlider;
class TabSliderButton;

// A view that is part of the CaptureBarView, from which the user can toggle
// between the two available capture types (image, and video).
class ASH_EXPORT CaptureModeTypeView : public views::View {
  METADATA_HEADER(CaptureModeTypeView, views::View)

 public:
  // The `active_behavior` decides the supported capture types.
  explicit CaptureModeTypeView(CaptureModeBehavior* active_behavior);
  CaptureModeTypeView(const CaptureModeTypeView&) = delete;
  CaptureModeTypeView& operator=(const CaptureModeTypeView&) = delete;
  ~CaptureModeTypeView() override;

  TabSliderButton* image_toggle_button() const { return image_toggle_button_; }
  TabSliderButton* video_toggle_button() const { return video_toggle_button_; }

  // Called when the capture type changes.
  void OnCaptureTypeChanged(CaptureModeType new_type);

 private:
  void OnImageToggle();
  void OnVideoToggle();

  // Owned by the views hierarchy. The capture type switch contains image and
  // video capture type toggle buttons.
  raw_ptr<TabSlider> capture_type_switch_;

  // Image and video toggle buttons are owned by `capture_type_switch_` which
  // will be created based on the active behavior of the current capture mode
  // session.
  raw_ptr<TabSliderButton> image_toggle_button_ = nullptr;
  raw_ptr<TabSliderButton> video_toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
