// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_context_menu.h"

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
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
  explicit MenuModelAdapter(ui::SimpleMenuModel* model)
      : views::MenuModelAdapter(model) {}

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
};

}  // namespace

DeskActionContextMenu::Config::Config() = default;
DeskActionContextMenu::Config::Config(Config&&) = default;
DeskActionContextMenu::Config::~Config() = default;
DeskActionContextMenu::Config& DeskActionContextMenu::Config::operator=(
    Config&&) = default;

DeskActionContextMenu::DeskActionContextMenu(Config config)
    : config_(std::move(config)), context_menu_model_(this) {
  bool separator_needed = false;
  auto maybe_add_separator = [&] {
    if (separator_needed) {
      context_menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
      separator_needed = false;
    }
  };

  if (config_.profiles.size() > 1) {
    for (size_t i = 0; i != config_.profiles.size(); ++i) {
      const auto& summary = config_.profiles[i];

      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          summary.icon, skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kIconProfileSize, kIconProfileSize));

      context_menu_model_.AddItemWithIcon(
          static_cast<int>(kDynamicProfileStart + i),
          base::UTF8ToUTF16(summary.name),
          ui::ImageModel::FromImageSkia(
              gfx::ImageSkiaOperations::CreateImageWithRoundRectClip(
                  kIconProfileSize, icon)));

      auto entry_index = context_menu_model_.GetItemCount() - 1;
      context_menu_model_.SetMinorText(entry_index,
                                       base::UTF8ToUTF16(summary.email));

      if (summary.profile_id == config_.current_lacros_profile_id) {
        context_menu_model_.SetMinorIcon(
            entry_index, ui::ImageModel::FromVectorIcon(
                             kHollowCheckCircleIcon,
                             cros_tokens::kCrosSysPrimary, kCheckButtonSize));
      }
    }

    context_menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_model_.AddItemWithIcon(
        CommandId::kShowProfileManager,
        l10n_util::GetStringUTF16(IDS_ASH_DESKS_OPEN_PROFILE_MANAGER),
        ui::ImageModel::FromVectorIcon(
            kSettingsIcon, cros_tokens::kCrosSysOnSurface, kCheckButtonSize));

    separator_needed = true;
  }

  if (config_.combine_desks_target_name) {
    maybe_add_separator();
    context_menu_model_.AddItemWithIcon(
        CommandId::kCombineDesks,
        l10n_util::GetStringFUTF16(IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION,
                                   *config_.combine_desks_target_name),
        ui::ImageModel::FromVectorIcon(kCombineDesksIcon,
                                       ui::kColorAshSystemUIMenuIcon));
  }

  if (config_.close_all_callback) {
    maybe_add_separator();
    context_menu_model_.AddItemWithIcon(
        CommandId::kCloseAll,
        l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION),
        ui::ImageModel::FromVectorIcon(kMediumOrLargeCloseButtonIcon,
                                       ui::kColorAshSystemUIMenuIcon));
  }

  menu_model_adapter_ =
      std::make_unique<MenuModelAdapter>(&context_menu_model_);
}

DeskActionContextMenu::~DeskActionContextMenu() = default;

void DeskActionContextMenu::MaybeCloseMenu() {
  if (context_menu_runner_)
    context_menu_runner_->Cancel();
}

void DeskActionContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
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
}

void DeskActionContextMenu::MaybeSetLacrosProfileId(int command_id) {
  size_t profile_index = static_cast<size_t>(command_id - kDynamicProfileStart);
  if (profile_index < config_.profiles.size()) {
    config_.set_lacros_profile_id.Run(
        config_.profiles[profile_index].profile_id);
  }
}

}  // namespace ash
