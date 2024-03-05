// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/style/checkbox.h"
#include "ash/style/switch.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

namespace {

// Creates a switch button to control showing/hiding the birch bar.
std::unique_ptr<Switch> CreateShowSuggestionSwitch(
    BirchBarMenuModelAdapter* model_adapter) {
  auto switch_button = std::make_unique<Switch>(base::BindRepeating(
      [](BirchBarMenuModelAdapter* model_adapter) {
        // TODO(zxdan): show/hide the birch bar via birch bar controller.
        //  Dismiss the menu.
        model_adapter->Cancel();
      },
      model_adapter));
  switch_button->SetIsOn(
      GetOverviewSession()
          ->GetGridWithRootWindow(model_adapter->root_window())
          ->IsBirchBarShowing());
  return switch_button;
}

}  // namespace

BirchBarMenuModelAdapter::BirchBarMenuModelAdapter(
    std::unique_ptr<BirchBarContextMenuModel> birch_menu_model,
    views::Widget* widget_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode)
    : AppMenuModelAdapter(std::string(),
                          std::move(birch_menu_model),
                          widget_owner,
                          source_type,
                          std::move(on_menu_closed_callback),
                          is_tablet_mode),
      root_window_(widget_owner->GetNativeWindow()->GetRootWindow()) {}

BirchBarMenuModelAdapter::~BirchBarMenuModelAdapter() = default;

void BirchBarMenuModelAdapter::OnButtonSelected(OptionButtonBase* button) {}

void BirchBarMenuModelAdapter::OnButtonClicked(OptionButtonBase* button) {
  button->SetSelected(!button->selected());
}

views::MenuItemView* BirchBarMenuModelAdapter::AppendMenuItem(
    views::MenuItemView* menu,
    ui::MenuModel* model,
    size_t index) {
  const int command_id = model->GetCommandIdAt(index);
  const std::u16string label = model->GetLabelAt(index);

  // Add switch button to show suggestions item.
  if (command_id ==
      base::to_underlying(
          BirchBarContextMenuModel::CommandId::kShowSuggestions)) {
    views::MenuItemView* item_view = menu->AppendMenuItem(command_id, label);
    auto* switch_button =
        item_view->AddChildView(CreateShowSuggestionSwitch(this));
    switch_button->SetAccessibleName(label);
    return item_view;
  }

  // Create customized checkbox item view for check items.
  if (model->GetTypeAt(index) == ui::SimpleMenuModel::ItemType::TYPE_CHECK) {
    views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
    // Note that we cannot directly added a checkbox, since `MenuItemView` will
    // align the newly added children to the right side of its label. We should
    // add a checkbox with the label text and remove menu's label by explicitly
    // setting an empty title.
    item_view->SetTitle(u"");
    // Since the checkbox is the only child, `MenuItemView` will treat the
    // current item view as a container and add container margins to the item.
    // To keep the checkbox preferred height, we should set the vertical margins
    // to 0.
    item_view->set_vertical_margin(0);
    // Creates a checkbox. The argument `button_width` is the minimum width of
    // the checkbox button. Since we are not going to limit the minimum size, so
    // it is set to 0.
    auto* checkbox = item_view->AddChildView(std::make_unique<Checkbox>(
        /*button_width=*/0, Checkbox::PressedCallback(),
        model->GetLabelAt(index)));
    checkbox->set_delegate(this);
    checkbox->SetAccessibleName(label);
    return item_view;
  }

  return AppMenuModelAdapter::AppendMenuItem(menu, model, index);
}

void BirchBarMenuModelAdapter::RecordHistogramOnMenuClosed() {
  // TODO(zxdan): add metrics later.
}

}  // namespace ash
