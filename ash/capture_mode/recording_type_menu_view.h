// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_
#define ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_

#include "ash/capture_mode/capture_mode_menu_group.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

// Defines a view that will be the contents view of the recording type menu
// widget, from which users can pick the desired recording format.
class RecordingTypeMenuView : public CaptureModeMenuGroup,
                              public CaptureModeMenuGroup::Delegate {
 public:
  RecordingTypeMenuView();
  RecordingTypeMenuView(const RecordingTypeMenuView&) = delete;
  RecordingTypeMenuView& operator=(const RecordingTypeMenuView&) = delete;
  ~RecordingTypeMenuView() override = default;

  // Returns the ideal bounds of the widget hosting this view, relative to the
  // `capture_label_widget_screen_bounds` which hosts the drop down button that
  // opens the recording type menu widget. If `contents_view` is provided, its
  // preferred size will be used, otherwise, the default size will be used.
  static gfx::Rect GetIdealScreenBounds(
      const gfx::Rect& capture_label_widget_screen_bounds,
      views::View* contents_view = nullptr);

  // CaptureModeMenuGroup::Delegate:
  void OnOptionSelected(int option_id) const override;
  bool IsOptionChecked(int option_id) const override;
  bool IsOptionEnabled(int option_id) const override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_
