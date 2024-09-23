// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_context_menu.h"

#include <string>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The size of desk profile icon.
constexpr int kIconProfileSize = 24;

// The size for selected profile checker icon.
constexpr int kCheckButtonSize = 20;

class MenuModelAdapter : public views::MenuModelAdapter {
 public:
  MenuModelAdapter(ui::SimpleMenuModel* model, base::WeakPtr<OverviewGrid> grid)
      : views::MenuModelAdapter(model), grid_(grid) {}

  views::MenuItemView* AppendMenuItem(views::MenuItemView* menu,
                                      ui::MenuModel* model,
                                      size_t index) override {
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      menu->AppendSeparator();
      return nullptr;
    }

    const int command_id = model->GetCommandIdAt(index);
    views::MenuItemView* item_view = menu->AppendMenuItem(command_id);

    if (command_id >= DeskActionContextMenu::kDynamicProfileStart) {
      item_view->SetSecondaryTitle(model->GetMinorTextAt(index));
    }

    item_view->SetIcon(model->GetIconAt(index));

    // The save desk option may be disabled if there are unsupported windows.
    if (command_id == DeskActionContextMenu::CommandId::kSaveAsTemplate ||
        command_id == DeskActionContextMenu::CommandId::kSaveForLater) {
      CHECK(grid_);
      SaveDeskOptionStatus status =
          grid_->GetEnableStateAndTooltipIDForTemplateType(
              command_id == DeskActionContextMenu::CommandId::kSaveAsTemplate
                  ? DeskTemplateType::kTemplate
                  : DeskTemplateType::kSaveAndRecall);
      menu->SetTooltip(l10n_util::GetStringUTF16(status.tooltip_id),
                       command_id);
    }

    // If the minor icon is set, then it's expected to be the checkmark used to
    // identify the currently selected desk profile. Note that simply doing
    // `item_view->SetMinorIcon` does not render the icon where we want it.
    if (auto minor_icon = model->GetMinorIconAt(index); !minor_icon.IsEmpty()) {
      item_view->AddChildView(
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kCenter)
              .AddChild(views::Builder<views::ImageView>().SetImage(
                  std::move(minor_icon)))
              .Build());
    }

    return item_view;
  }

 private:
  // Used to get the enabled/disabled status and tooltip.
  base::WeakPtr<OverviewGrid> grid_;
};

}  // namespace

DeskActionContextMenu::Config::Config() = default;
DeskActionContextMenu::Config::Config(Config&&) = default;
DeskActionContextMenu::Config::~Config() = default;
DeskActionContextMenu::Config& DeskActionContextMenu::Config::operator=(
    Config&&) = default;

DeskActionContextMenu::DeskActionContextMenu(Config config,
                                             DeskMiniView* mini_view)
    : config_(std::move(config)),
      mini_view_(mini_view),
      context_menu_model_(this) {
  bool separator_needed = false;
  auto maybe_add_separator = [&] {
    if (separator_needed) {
      context_menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
      separator_needed = false;
    }
  };

  // Set the accessible name for menu items here and then pipe the information
  // to the view instances during `ShowContextMenuForViewImpl()`.

  if (config_.profiles.size() > 1) {
    for (size_t i = 0; i != config_.profiles.size(); ++i) {
      const auto& summary = config_.profiles[i];

      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          summary.icon, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kIconProfileSize, kIconProfileSize));

      context_menu_model_.AddItemWithIcon(
          static_cast<int>(kDynamicProfileStart + i), summary.name,
          ui::ImageModel::FromImageSkia(
              gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
                  kIconProfileSize, icon)));

      auto entry_index = context_menu_model_.GetItemCount() - 1;
      context_menu_model_.SetMinorText(entry_index, summary.email);

      int profile_a11y_id = IDS_ASH_DESKS_MENU_ITEM_PROFILE_NOT_CHECKED;
      if (summary.profile_id == config_.current_lacros_profile_id) {
        context_menu_model_.SetMinorIcon(
            entry_index, ui::ImageModel::FromVectorIcon(
                             kHollowCheckCircleIcon,
                             cros_tokens::kCrosSysPrimary, kCheckButtonSize));
        profile_a11y_id = IDS_ASH_DESKS_MENU_ITEM_PROFILE_CHECKED;
      }

      context_menu_model_.SetAccessibleNameAt(
          entry_index, l10n_util::GetStringFUTF16(profile_a11y_id, summary.name,
                                                  summary.email));
    }

    context_menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    const std::u16string profile_manager_a11y =
        l10n_util::GetStringUTF16(IDS_ASH_DESKS_OPEN_PROFILE_MANAGER);
    context_menu_model_.AddItemWithIcon(
        CommandId::kShowProfileManager, profile_manager_a11y,
        ui::ImageModel::FromVectorIcon(
            kSettingsIcon, cros_tokens::kCrosSysOnSurface, kCheckButtonSize));
    context_menu_model_.SetAccessibleNameAt(
        context_menu_model_.GetItemCount() - 1, profile_manager_a11y);

    separator_needed = true;
  }

  // Accessible names should be set on each of the following menu items to
  // ensure the labels are read properly by screen readers.

  if (config_.save_template_target_name) {
    maybe_add_separator();
    const std::u16string save_template_a11y = l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_AS_TEMPLATE_BUTTON);
    context_menu_model_.AddItemWithIcon(
        CommandId::kSaveAsTemplate, save_template_a11y,
        ui::ImageModel::FromVectorIcon(kSaveDeskAsTemplateIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    context_menu_model_.SetAccessibleNameAt(
        context_menu_model_.GetItemCount() - 1, save_template_a11y);

    // The save desk options may be disabled if there are unsupported windows.
    OverviewSession* session = GetOverviewSession();
    CHECK(session);
    OverviewGrid* grid =
        session->GetGridWithRootWindow(mini_view_->root_window());
    CHECK(grid);
    context_menu_model_.SetEnabledAt(
        context_menu_model_.GetItemCount() - 1,
        grid->GetEnableStateAndTooltipIDForTemplateType(
                DeskTemplateType::kTemplate)
            .enabled);
  }

  if (config_.save_later_target_name) {
    maybe_add_separator();
    const std::u16string save_later_a11y = l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_FOR_LATER_BUTTON);
    context_menu_model_.AddItemWithIcon(
        CommandId::kSaveForLater, save_later_a11y,
        ui::ImageModel::FromVectorIcon(kSaveDeskForLaterIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    context_menu_model_.SetAccessibleNameAt(
        context_menu_model_.GetItemCount() - 1, save_later_a11y);

    // The save desk options may be disabled if there are unsupported windows.
    OverviewSession* session = GetOverviewSession();
    CHECK(session);
    OverviewGrid* grid =
        session->GetGridWithRootWindow(mini_view_->root_window());
    CHECK(grid);
    context_menu_model_.SetEnabledAt(
        context_menu_model_.GetItemCount() - 1,
        grid->GetEnableStateAndTooltipIDForTemplateType(
                DeskTemplateType::kSaveAndRecall)
            .enabled);
  }

  if (config_.combine_desks_target_name) {
    maybe_add_separator();
    const std::u16string combine_desks_a11y =
        l10n_util::GetStringFUTF16(IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION,
                                   *config_.combine_desks_target_name);
    context_menu_model_.AddItemWithIcon(
        CommandId::kCombineDesks, combine_desks_a11y,
        ui::ImageModel::FromVectorIcon(kCombineDesksIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    context_menu_model_.SetAccessibleNameAt(
        context_menu_model_.GetItemCount() - 1, combine_desks_a11y);
  }

  if (config_.close_all_target_name) {
    maybe_add_separator();
    const std::u16string close_all_a11y = l10n_util::GetStringFUTF16(
        IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION, *config_.close_all_target_name);
    context_menu_model_.AddItemWithIcon(
        CommandId::kCloseAll, close_all_a11y,
        ui::ImageModel::FromVectorIcon(kMediumOrLargeCloseButtonIcon,
                                       ui::kColorAshSystemUIMenuIcon));
    context_menu_model_.SetAccessibleNameAt(
        context_menu_model_.GetItemCount() - 1, close_all_a11y);
  }

  OverviewGrid* grid = mini_view_->owner_bar()->overview_grid();
  menu_model_adapter_ = std::make_unique<MenuModelAdapter>(
      &context_menu_model_, grid ? grid->GetWeakPtr() : nullptr);
}

DeskActionContextMenu::~DeskActionContextMenu() = default;

void DeskActionContextMenu::MaybeCloseMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

void DeskActionContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case CommandId::kSaveAsTemplate:
      config_.save_template_callback.Run();
      break;
    case CommandId::kSaveForLater:
      config_.save_later_callback.Run();
      break;
    case CommandId::kCombineDesks:
      config_.combine_desks_callback.Run();
      break;
    case CommandId::kCloseAll:
      config_.close_all_callback.Run();
      break;
    case CommandId::kShowProfileManager:
      base::UmaHistogramBoolean(kDeskProfilesOpenProfileManagerHistogramName,
                                true);
      Shell::Get()->shell_delegate()->OpenProfileManager();
      break;
    default:
      MaybeSetLacrosProfileId(command_id);
      break;
  }
}

void DeskActionContextMenu::MenuClosed(ui::SimpleMenuModel* menu) {
  if (config_.on_context_menu_closed_callback) {
    config_.on_context_menu_closed_callback.Run();
  }
}

void DeskActionContextMenu::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR |
                        views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;

  auto menu_root = menu_model_adapter_->CreateMenu();
  root_menu_item_view_ = menu_root.get();

  context_menu_runner_ =
      std::make_unique<views::MenuRunner>(std::move(menu_root), run_types);
  context_menu_runner_->RunMenuAt(source->GetWidget(),
                                  /*button_controller=*/nullptr,
                                  /*bounds=*/gfx::Rect(point, gfx::Size()),
                                  config_.anchor_position, source_type);

  // Pipe accessible names from the menu model to the view instances. Please
  // note, a separator in menu model does *not* end up being represented by a
  // view instance.
  auto item_views = root_menu_item_view_->GetSubmenu()->GetMenuItems();
  const size_t model_count = context_menu_model_.GetItemCount();
  CHECK_LE(item_views.size(), model_count);
  for (size_t view_index = 0, model_index = 0; model_index < model_count;
       model_index++) {
    if (auto a11y_name = context_menu_model_.GetAccessibleNameAt(model_index);
        !a11y_name.empty()) {
      item_views[view_index++]->GetViewAccessibility().SetName(a11y_name);
    }
  }
}

void DeskActionContextMenu::MaybeSetLacrosProfileId(int command_id) {
  size_t profile_index = static_cast<size_t>(command_id - kDynamicProfileStart);
  if (profile_index < config_.profiles.size()) {
    config_.set_lacros_profile_id.Run(
        config_.profiles[profile_index].profile_id);
  }
}

}  // namespace ash
