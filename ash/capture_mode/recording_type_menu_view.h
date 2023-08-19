// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_
#define ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "base/functional/callback_forward.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class SystemShadow;

// Defines a view that will be the contents view of the recording type menu
// widget, from which users can pick the desired recording format.
class ASH_EXPORT RecordingTypeMenuView : public CaptureModeMenuGroup,
                                         public CaptureModeMenuGroup::Delegate {
 public:
  explicit RecordingTypeMenuView(
      base::RepeatingClosure on_option_selected_callback);
  RecordingTypeMenuView(const RecordingTypeMenuView&) = delete;
  RecordingTypeMenuView& operator=(const RecordingTypeMenuView&) = delete;
  ~RecordingTypeMenuView() override;

  // Returns the ideal bounds of the widget hosting this view, relative to the
  // `capture_label_widget_screen_bounds` which hosts the drop down button that
  // opens the recording type menu widget. `target_display_screen_bounds` will
  // be used to ensure the resulting bounds are contained within the target
  // display. If `contents_view` is provided, its preferred size will be used,
  // otherwise, the default size will be used.
  static gfx::Rect GetIdealScreenBounds(
      const gfx::Rect& capture_label_widget_screen_bounds,
      const gfx::Rect& target_display_screen_bounds,
      views::View* contents_view = nullptr);

  // CaptureModeMenuGroup::Delegate:
  void OnOptionSelected(int option_id) const override;
  bool IsOptionChecked(int option_id) const override;
  bool IsOptionEnabled(int option_id) const override;

  views::View* GetWebMOptionForTesting();
  views::View* GetGifOptionForTesting();

 private:
  // A callback that will be triggered after an option has been selected, and
  // the recording type has been set on the `CaptureModeController`. This is
  // bound to a function in the `CaptureModeSession` that closes the menu.
  base::RepeatingClosure on_option_selected_callback_;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_RECORDING_TYPE_MENU_VIEW_H_
