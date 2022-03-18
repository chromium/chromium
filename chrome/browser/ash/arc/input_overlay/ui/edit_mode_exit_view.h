// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_MODE_EXIT_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_MODE_EXIT_VIEW_H_

#include <memory>

#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

namespace ash {
class PillButton;
}  // namespace ash

// View displaying the 3 possible options to exit edit mode.
//
// These actions refer to what the user can do wrt customized key-bindings, they
// can either reset to a set of default key-bindings or just accept/cancel the
// ongoing changes.
//
// View looks like this:
// +----------------------+
// |   Reset to defaults  |
// |                      |
// |         Save         |
// |                      |
// |        Cancel        |
// +----------------------+

namespace arc {
namespace input_overlay {

class DisplayOverlayController;

class EditModeExitView : public views::View {
 public:
  static std::unique_ptr<EditModeExitView> BuildView(
      DisplayOverlayController* display_overlay_controller,
      gfx::Point position);

  explicit EditModeExitView(
      DisplayOverlayController* display_overlay_controller);

  EditModeExitView(const EditModeExitView&) = delete;
  EditModeExitView& operator=(const EditModeExitView&) = delete;
  ~EditModeExitView() override;

 private:
  void Init(gfx::Point position);

  void OnResetButtonPressed();
  void OnSaveButtonPressed();
  void OnCancelButtonPressed();

  ash::PillButton* reset_button_ = nullptr;
  ash::PillButton* save_button_ = nullptr;
  ash::PillButton* cancel_button_ = nullptr;

  // DisplayOverlayController owns |this| class, no need to deallocate.
  DisplayOverlayController* const display_overlay_controller_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_MODE_EXIT_VIEW_H_
