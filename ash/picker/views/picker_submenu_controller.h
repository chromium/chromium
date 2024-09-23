// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_
#define ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class PickerListItemView;
class PickerSubmenuView;

class ASH_EXPORT PickerSubmenuController : public views::ViewObserver {
 public:
  PickerSubmenuController();
  PickerSubmenuController(const PickerSubmenuController&) = delete;
  PickerSubmenuController& operator=(const PickerSubmenuController&) = delete;
  ~PickerSubmenuController() override;

  // ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // Shows the submenu with `items`, anchoring it to `anchor_view`.
  // If this submenu is already showing, then it is closed first before showing
  // the new items.
  void Show(views::View* anchor_view,
            std::vector<std::unique_ptr<PickerListItemView>> items);

  // Closes the submenu.
  void Close();

  // Gets the currently showing submenu view, or returns nullptr if no submenu
  // is showing.
  PickerSubmenuView* GetSubmenuView();

  // Gets the anchor view for the currently showing submenu, or returns nullptr
  // if no submenu is showing.
  views::View* GetAnchorView();

  views::Widget* widget_for_testing() { return widget_.get(); }

 private:
  views::UniqueWidgetPtr widget_;

  raw_ptr<views::View> anchor_view_;

  base::ScopedObservation<views::View, views::ViewObserver>
      anchor_view_observation_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SUBMENU_CONTROLLER_H_
