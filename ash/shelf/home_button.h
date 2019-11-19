// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_HOME_BUTTON_H_
#define ASH_SHELF_HOME_BUTTON_H_

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/ash_export.h"
#include "ash/shelf/home_button_controller.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_control_button.h"
#include "base/macros.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

class ShelfButtonDelegate;

// Button used for the AppList icon on the shelf. It opens the app list (in
// clamshell mode) or home screen (in tablet mode). Because the clamshell-mode
// app list appears like a dismissable overlay, the button is highlighted while
// the app list is open in clamshell mode.
//
// If Assistant is enabled, the button is filled in; long-pressing it will
// launch Assistant.
class ASH_EXPORT HomeButton : public ShelfControlButton,
                              public ShelfButtonDelegate,
                              public views::ViewTargeterDelegate {
 public:
  static const char kViewClassName[];

  explicit HomeButton(Shelf* shelf);
  ~HomeButton() override;

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;
  const char* GetClassName() const override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

  // Called when the availability of a long-press gesture may have changed, e.g.
  // when Assistant becomes enabled.
  void OnAssistantAvailabilityChanged();

  // True if the app list is shown for the display containing this button.
  bool IsShowingAppList() const;

  virtual void OnPressed(AppListShowSource show_source,
                         base::TimeTicks time_stamp);

  // Returns the display which contains this view.
  int64_t GetDisplayId() const;

 protected:
  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // The controller used to determine the button's behavior.
  HomeButtonController controller_;

  DISALLOW_COPY_AND_ASSIGN(HomeButton);
};

}  // namespace ash

#endif  // ASH_SHELF_HOME_BUTTON_H_
