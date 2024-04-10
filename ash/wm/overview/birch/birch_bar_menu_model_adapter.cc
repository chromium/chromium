// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_menu_model_adapter.h"

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/style/checkbox.h"
#include "ash/style/switch.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/prefs/pref_service.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

namespace {

// Returns the pref service to use for Birch bar prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Creates a switch button to control showing/hiding the birch bar.
std::unique_ptr<Switch> CreateShowSuggestionSwitch() {
  auto switch_button =
      std::make_unique<Switch>(base::BindRepeating([]() {
        auto* birch_bar_controller = BirchBarController::Get();
        CHECK(birch_bar_controller);

        // Note that the menu should be dismissed before changing the show
        // suggestions pref which map destroy the chips.
        views::MenuController::GetActiveInstance()->Cancel(
            views::MenuController::ExitType::kAll);

        birch_bar_controller->SetShowBirchSuggestions(
            /*show=*/!birch_bar_controller->GetShowBirchSuggestions());
      }));
  switch_button->SetIsOn(BirchBarController::Get()->GetShowBirchSuggestions());
  return switch_button;
}

// Get suggestion pref name from the given command Id.
std::string CommandIdToSuggestionPrefName(int command_id) {
  switch (command_id) {
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kCalendarSuggestions):
      return prefs::kBirchUseCalendar;
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kWeatherSuggestions):
      return prefs::kBirchUseWeather;
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kDriveSuggestions):
      return prefs::kBirchUseFileSuggest;
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kOtherDeviceSuggestions):
      return prefs::kBirchUseRecentTabs;
    default:
      break;
  }
  NOTREACHED_NORETURN() << "No matching suggestion type for command Id: "
                        << command_id;
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
                          is_tablet_mode) {}

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

  switch (command_id) {
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kShowSuggestions): {
      views::MenuItemView* item_view = menu->AppendMenuItem(command_id, label);
      auto* switch_button =
          item_view->AddChildView(CreateShowSuggestionSwitch());
      switch_button->SetAccessibleName(label);
      return item_view;
    }
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kWeatherSuggestions):
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kCalendarSuggestions):
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kDriveSuggestions):
    case base::to_underlying(
        BirchBarContextMenuModel::CommandId::kOtherDeviceSuggestions): {
      views::MenuItemView* item_view = menu->AppendMenuItem(command_id);
      // Note that we cannot directly added a checkbox, since `MenuItemView`
      // will align the newly added children to the right side of its label. We
      // should add a checkbox with the label text and remove menu's label by
      // explicitly setting an empty title.
      item_view->SetTitle(u"");
      // Since the checkbox is the only child, `MenuItemView` will treat the
      // current item view as a container and add container margins to the item.
      // To keep the checkbox preferred height, we should set the vertical
      // margins to 0.
      item_view->set_vertical_margin(0);
      // Creates a checkbox. The argument `button_width` is the minimum width of
      // the checkbox button. Since we are not going to limit the minimum size,
      // so it is set to 0.
      auto* checkbox = item_view->AddChildView(std::make_unique<Checkbox>(
          /*button_width=*/0,
          base::BindRepeating(
              [](int command_id, bool close_menu) {
                // To avoid UAF, dismiss the menu before changing the pref which
                // would destroy current chips.
                if (close_menu) {
                  views::MenuController::GetActiveInstance()->Cancel(
                      views::MenuController::ExitType::kAll);
                }

                auto* pref_service = GetPrefService();
                const std::string pref_name =
                    CommandIdToSuggestionPrefName(command_id);
                pref_service->SetBoolean(pref_name,
                                         !pref_service->GetBoolean(pref_name));
              },
              command_id, close_menu_on_customizing_suggestions_),
          model->GetLabelAt(index)));
      checkbox->SetSelected(GetPrefService()->GetBoolean(
          CommandIdToSuggestionPrefName(command_id)));
      checkbox->set_delegate(this);
      checkbox->SetAccessibleName(label);
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
