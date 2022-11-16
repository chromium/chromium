// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"

// View displaying the 3 possible options to finish edit mode.
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

namespace arc::input_overlay {

class DisplayOverlayController;

class EditFinishView : public views::View {
 public:
  static std::unique_ptr<EditFinishView> BuildView(
      DisplayOverlayController* display_overlay_controller,
      const gfx::Size& parent_size);

  explicit EditFinishView(DisplayOverlayController* display_overlay_controller);

  EditFinishView(const EditFinishView&) = delete;
  EditFinishView& operator=(const EditFinishView&) = delete;
  ~EditFinishView() override;

 private:
  class ChildButton;

  void Init(const gfx::Size& parent_size);
  int CalculateWidth();
  void OnResetButtonPressed();
  void OnSaveButtonPressed();
  void OnCancelButtonPressed();

  raw_ptr<ChildButton> reset_button_ = nullptr;
  raw_ptr<ChildButton> save_button_ = nullptr;
  raw_ptr<ChildButton> cancel_button_ = nullptr;

  // DisplayOverlayController owns |this| class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_FINISH_VIEW_H_
