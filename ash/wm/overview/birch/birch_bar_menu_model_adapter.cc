// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/shell.h"
#include "ash/style/checkbox.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography_provider.h"

namespace ash {

constexpr gfx::Size kShowSuggestionsItemSize(304, 32);

using CommandId = BirchBarContextMenuModel::CommandId;

BirchBarMenuModelAdapter::BirchBarMenuModelAdapter(
    std::unique_ptr<ui::SimpleMenuModel> birch_menu_model,
    views::Widget* widget_owner,
    ui::MenuSourceType source_type,
    base::OnceClosure on_menu_closed_callback,
    bool is_tablet_mode,
    bool for_chip_menu)
    : AppMenuModelAdapter(std::string(),
                          std::move(birch_menu_model),
                          widget_owner,
                          source_type,
                          std::move(on_menu_closed_callback),
                          is_tablet_mode),
      for_chip_menu_(for_chip_menu) {}

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
  const gfx::FontList font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_TOUCH_MENU, views::style::STYLE_PRIMARY);
  const int menu_item_padding =
      views::MenuConfig::instance().touchable_item_horizontal_padding;
  switch (command_id) {
    case base::to_underlying(CommandId::kShowSuggestions): {
      // By default, all menu item labels will start after the icon column. To
      // make the show suggestions label left aligned, we cannot use the menu
      // item label but create a new label and add it in a container with the
      // switch button.
      views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
      item_view->SetTitle(std::u16string());
      item_view->SetHighlightWhenSelectedWithChildViews(true);

      // Set the name so that this is compatible with
      // `MenuItemView::GetAccessibleNodeData()`.
      item_view->GetViewAccessibility().SetName(label);

      // Create a container with the show suggestions label, a spacer, and the
      // switch button.
      views::View* spacer;
      auto* switch_container = item_view->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetPreferredSize(kShowSuggestionsItemSize)
              .SetInsideBorderInsets(
                  gfx::Insets::TLBR(0, menu_item_padding, 0, 0))
              .AddChildren(
                  views::Builder<views::Label>().SetText(label).SetFontList(
                      font_list),
                  views::Builder<views::View>().CopyAddressTo(&spacer))
              .Build());

      // Make the spacer fill in the middle space to make the label left aligned
      // and the switch button right aligned.
      switch_container->SetFlexForView(spacer, 1);

      auto switch_button = std::make_unique<Switch>(base::BindRepeating(
          [](bool from_chip) {
            auto* birch_bar_controller = BirchBarController::Get();
            CHECK(birch_bar_controller);
            birch_bar_controller->ExecuteMenuCommand(
                base::to_underlying(CommandId::kShowSuggestions), from_chip);
          },
          for_chip_menu_));
      switch_button->SetIsOn(
          BirchBarController::Get()->GetShowBirchSuggestions());
      switch_button->GetViewAccessibility().SetName(label);

      switch_container->AddChildView(std::move(switch_button));
      return item_view;
    }
    case base::to_underlying(CommandId::kWeatherSuggestions):
    case base::to_underlying(CommandId::kCalendarSuggestions):
    case base::to_underlying(CommandId::kDriveSuggestions):
    case base::to_underlying(CommandId::kChromeTabSuggestions):
    case base::to_underlying(CommandId::kMediaSuggestions):
    case base::to_underlying(CommandId::kCoralSuggestions): {
      views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
      // Note that we cannot directly added a checkbox, since `MenuItemView`
      // will align the newly added children to the right side of its label. We
      // should add a checkbox with the label text and remove menu's label by
      // explicitly setting an empty title.
      item_view->SetTitle(std::u16string());
      // Since the checkbox is the only child, `MenuItemView` will treat the
      // current item view as a container and add container margins to the item.
      // To keep the checkbox preferred height, we should set the vertical
      // margins to 0.
      item_view->set_vertical_margin(0);
      item_view->SetHighlightWhenSelectedWithChildViews(true);
      item_view->GetViewAccessibility().SetName(label);

      // Creates a checkbox. The argument `button_width` is the minimum width of
      // the checkbox button. Since we are not going to limit the minimum size,
      // so it is set to 0.
      auto* checkbox = item_view->AddChildView(std::make_unique<Checkbox>(
          /*button_width=*/0,
          base::BindRepeating(
              [](int command_id, bool from_chip) {
                BirchBarController::Get()->ExecuteMenuCommand(command_id,
                                                              from_chip);
              },
              command_id, for_chip_menu_),
          label, gfx::Insets::VH(0, menu_item_padding), menu_item_padding));
      bool enabled = item_view->GetEnabled();
      checkbox->SetEnabled(enabled);
      checkbox->SetSelected(
          enabled &&
          BirchBarController::Get()->GetShowSuggestionType(
              birch_bar_util::CommandIdToSuggestionType(command_id)));
      checkbox->set_delegate(this);
      checkbox->GetViewAccessibility().SetName(label);
      checkbox->SetLabelFontList(font_list);
      checkbox->SetLabelColorId(cros_tokens::kCrosSysOnSurface);
      // Checkboxes don't support minor text, so we use minor text for tooltip.
      // Note that most commands do not have minor text / tooltips.
      checkbox->SetTooltipText(model->GetMinorTextAt(index));
      return item_view;
    }
    default:
      break;
  }

  return AppMenuModelAdapter::AppendMenuItem(menu, model, index);
}

void BirchBarMenuModelAdapter::RecordHistogramOnMenuClosed() {
  // TODO(zxdan): add metrics later.
}

}  // namespace ash
