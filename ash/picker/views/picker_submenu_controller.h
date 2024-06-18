// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_
#define ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class PickerItemView;

class ASH_EXPORT PickerSubmenuController {
 public:
  PickerSubmenuController();
  PickerSubmenuController(const PickerSubmenuController&) = delete;
  PickerSubmenuController& operator=(const PickerSubmenuController&) = delete;
  ~PickerSubmenuController();

  // Shows the submenu with `items`, anchoring it to `anchor_view`.
  // If this submenu is already showing, then it is closed first before showing
  // the new items.
  void Show(views::View* anchor_view,
            std::vector<std::unique_ptr<PickerItemView>> items);

  // Closes the submenu.
  void Close();

  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  views::UniqueWidgetPtr widget_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_
