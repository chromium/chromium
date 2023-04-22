// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Separator;
}  // namespace views

namespace ash {

class IconButton;
class CaptureModeSourceView;
class CaptureModeTypeView;
class SystemShadow;

// A view that acts as the content view of the capture mode bar widget.
// It has a set of buttons to toggle between image and video capture, and
// another set of buttons to toggle between fullscreen, region, and window
// capture sources. It also contains a settings button. The structure looks like
// this:
//
//   +---------------------------------------------------------------+
//   |  +----------------+  |                       |                |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  |  |   |  |   |  |  |  |   |  |   |  |   |  |  |   |  |   |  |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  +----------------+  |  ^                 ^  |  ^      ^      |
//   +--^----------------------|-----------------|-----|------|------+
//   ^  |                      +-----------------+     |      |
//   |  |                      |                       |      IconButton
//   |  |                      |                       |
//   |  |                      |                       IconButton
//   |  |                      CaptureModeSourceView
//   |  CaptureModeTypeView
//   |
//   CaptureModeBarView
//
class ASH_EXPORT CaptureModeBarView : public views::View {
 public:
  METADATA_HEADER(CaptureModeBarView);

  // The `active_behavior` decides the capture bar configurations.
  explicit CaptureModeBarView(CaptureModeBehavior* active_behavior);
  CaptureModeBarView(const CaptureModeBarView&) = delete;
  CaptureModeBarView& operator=(const CaptureModeBarView&) = delete;
  ~CaptureModeBarView() override;

  CaptureModeTypeView* capture_type_view() const { return capture_type_view_; }
  CaptureModeSourceView* capture_source_view() const {
    return capture_source_view_;
  }
  IconButton* settings_button() const { return settings_button_; }
  IconButton* close_button() const { return close_button_; }

  // Gets the ideal bounds in screen coordinates of the bar of widget on the
  // given `root` window. The `image_toggle_button` will not be shown in the bar
  // The width of the bar will be adjusted based on the current active behavior
  static gfx::Rect GetBounds(aura::Window* root,
                             CaptureModeBehavior* active_behavior);

  // Called when either the capture mode source or type changes.
  void OnCaptureSourceChanged(CaptureModeSource new_source);
  void OnCaptureTypeChanged(CaptureModeType new_type);

  // Called when settings is toggled on or off.
  void SetSettingsMenuShown(bool shown);

 private:
  void OnSettingsButtonPressed(const ui::Event& event);
  void OnCloseButtonPressed();

  // Owned by the views hierarchy.
  raw_ptr<CaptureModeTypeView, ExperimentalAsh> capture_type_view_;
  raw_ptr<views::Separator, ExperimentalAsh> separator_1_;
  raw_ptr<CaptureModeSourceView, ExperimentalAsh> capture_source_view_;
  raw_ptr<views::Separator, ExperimentalAsh> separator_2_;
  raw_ptr<IconButton, ExperimentalAsh> settings_button_;
  raw_ptr<IconButton, ExperimentalAsh> close_button_;
  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BAR_VIEW_H_
