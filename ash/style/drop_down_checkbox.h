// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DROP_DOWN_CHECKBOX_H_
#define ASH_STYLE_DROP_DOWN_CHECKBOX_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// A label button has a drop-down list of checkboxes that can be accessed by
// clicking the arrow button. The data model of the drop down checkboxes is a
// `ui::ListModel` with `std::u16string` type.
class ASH_EXPORT DropDownCheckbox : public views::Button,
                                    public views::WidgetObserver {
  METADATA_HEADER(DropDownCheckbox, views::Button)

 public:
  using ItemModel = ui::ListModel<std::u16string>;
  using SelectedIndices = ui::ListSelectionModel::SelectedIndices;
  using SelectedItems = std::vector<std::u16string>;

  DropDownCheckbox(const std::u16string& title, ItemModel* model);
  DropDownCheckbox(const DropDownCheckbox&) = delete;
  DropDownCheckbox& operator=(const DropDownCheckbox&) = delete;
  ~DropDownCheckbox() override;

  // The action which will be taken after the drop-down menu is closed.
  void SetSelectedAction(base::RepeatingClosure callback);

  SelectedIndices GetSelectedIndices() const;
  SelectedItems GetSelectedItems() const;

  // Returns whether or not the menu is currently running.
  bool IsMenuRunning() const;

  // views::Button:
  void SetCallback(PressedCallback callback) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnBlur() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void Layout(PassKey) override;

  // WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override;

 private:
  class SelectionModel;
  class MenuView;
  class EventHandler;

  // Gets expected menu bounds according to combox location.
  gfx::Rect GetExpectedMenuBounds() const;

  // Called when the arrow button is pressed.
  void OnDropDownCheckboxPressed();

  // Shows/Closes the drop down menu.
  void ShowDropDownMenu();
  void CloseDropDownMenu();

  // Called when the selections are made and the menu is closed.
  void OnPerformAction();

  // Reference to the data model.
  raw_ptr<ItemModel> model_;

  const raw_ptr<views::Label> title_ = nullptr;
  const raw_ptr<views::ImageView> drop_down_arrow_ = nullptr;

  // The selection model that manages the selections.
  std::unique_ptr<SelectionModel> selection_model_;

  // The action that will be taken after closing the drop down menu.
  base::RepeatingClosure callback_;

  // A handler handles mouse and touch event happening outside drop down
  // checkbox. This is mainly used to decide if we should close it.
  std::unique_ptr<EventHandler> event_handler_;

  // Drop down menu view owned by menu widget.
  raw_ptr<MenuView> menu_view_ = nullptr;

  // Drop down menu widget.
  views::UniqueWidgetPtr menu_;

  // The last time that the menu closed. This is used to avoid re-opening the
  // menu while clicking on the arrow button to close the menu.
  base::TimeTicks closed_time_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observer_{this};
};

}  // namespace ash

#endif  // ASH_STYLE_DROP_DOWN_CHECKBOX_H_
