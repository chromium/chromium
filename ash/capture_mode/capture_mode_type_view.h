// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class CaptureModeToggleButton;

// A view that is part of the CaptureBarView, from which the user can toggle
// between the two available capture types (image, and video).
class ASH_EXPORT CaptureModeTypeView : public views::View {
 public:
  METADATA_HEADER(CaptureModeTypeView);

  // |projector_mode| specifies whether the current capture mode session was
  // started for the projector workflow. In this mode, only video recording is
  // allowed.
  explicit CaptureModeTypeView(bool projector_mode);
  CaptureModeTypeView(const CaptureModeTypeView&) = delete;
  CaptureModeTypeView& operator=(const CaptureModeTypeView&) = delete;
  ~CaptureModeTypeView() override;

  CaptureModeToggleButton* image_toggle_button() const {
    return image_toggle_button_;
  }
  CaptureModeToggleButton* video_toggle_button() const {
    return video_toggle_button_;
  }

  // Called when the capture type changes.
  void OnCaptureTypeChanged(CaptureModeType new_type);

 private:
  void OnImageToggle();
  void OnVideoToggle();

  // Owned by the views hierarchy. Note that `image_toggle_button_` is
  // conditionally created based on the value of `projector_mode`, since it's
  // only possible to capture videos in the Projector-initiated sessions.
  CaptureModeToggleButton* image_toggle_button_;
  CaptureModeToggleButton* video_toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPE_VIEW_H_
