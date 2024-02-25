// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_FRAME_CONTEXT_MENU_CONTROLLER_H_
#define ASH_FRAME_FRAME_CONTEXT_MENU_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/context_menu_controller.h"

namespace chromeos {
class MoveToDesksMenuModel;
}  // namespace chromeos

namespace views {
class View;
class Widget;
class MenuRunner;
}  // namespace views

namespace ash {

// FrameContextMenuController is used to house the common code for displaying
// the context menu of frames like `NonClientFrameViewAsh` and `WideFrameView`.
class ASH_EXPORT FrameContextMenuController
    : public views::ContextMenuController {
 public:
  class Delegate {
   public:
    // Given a `source` and a `screen_coords_point`, determine whether the
    // context menu should be shown.
    virtual bool ShouldShowContextMenu(
        views::View* source,
        const gfx::Point& screen_coords_point) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  FrameContextMenuController(views::Widget* frame, Delegate* delegate);
  FrameContextMenuController(const FrameContextMenuController&) = delete;
  FrameContextMenuController& operator=(const FrameContextMenuController&) =
      delete;
  ~FrameContextMenuController() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

 private:
  // The widget that `this` controls the context menu for.
  raw_ptr<views::Widget> frame_;

  // A delegate who is responsible for determining whether the context menu
  // should be shown at a point.
  raw_ptr<Delegate> delegate_;

  std::unique_ptr<chromeos::MoveToDesksMenuModel> move_to_desks_menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace ash

#endif  // ASH_FRAME_FRAME_CONTEXT_MENU_CONTROLLER_H_
