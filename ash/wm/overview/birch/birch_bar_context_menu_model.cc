// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/menu/menu_types.h"

namespace ash {

using CommandId = BirchBarContextMenuModel::CommandId;

namespace {

// Generates and stylizes the icon for menu item.
ui::ImageModel CreateIcon(const gfx::VectorIcon& icon) {
  constexpr ui::ColorId kMenuIconColorId = cros_tokens::kCrosSysOnSurface;
  constexpr int kMenuIconSize = 20;
  return ui::ImageModel::FromVectorIcon(icon, kMenuIconColorId, kMenuIconSize);
}

}  // namespace

BirchBarContextMenuModel::BirchBarContextMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BirchBarContextMenuModel::Type type)
    : ui::SimpleMenuModel(delegate), type_(type) {
  // Fill in the items according to the menu type.
  switch (type) {
    case Type::kCollapsedBarMenu:
    case Type::kExpandedBarMenu:
      AddBarMenuItems();
      break;
    case Type::kChipMenu:
      AddChipMenuItems();
      break;
  }
}

BirchBarContextMenuModel::~BirchBarContextMenuModel() = default;

void BirchBarContextMenuModel::AddBarMenuItems() {
  CHECK(type_ == Type::kExpandedBarMenu || type_ == Type::kCollapsedBarMenu);

  // Show suggestions option is in both expanded and collapsed menu.
  AddItem(base::to_underlying(CommandId::kShowSuggestions),
          u"Show suggestions");

  // Expanded menu also has customizing suggestions options.
  if (type_ == Type::kExpandedBarMenu) {
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItem(base::to_underlying(CommandId::kWeatherSuggestions), u"Weather");
    AddItem(base::to_underlying(CommandId::kCalendarSuggestions),
            u"Google Calendar");
    AddItem(base::to_underlying(CommandId::kDriveSuggestions), u"Google Drive");
    AddItem(base::to_underlying(CommandId::kOtherDeviceSuggestions),
            u"Chrome from other devices");
    AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
    AddItemWithIcon(base::to_underlying(CommandId::kReset), u"Reset",
                    CreateIcon(kResetIcon));
  }
}

void BirchBarContextMenuModel::AddChipMenuItems() {
  CHECK(type_ == Type::kChipMenu);
  sub_menu_model_ = std::make_unique<BirchBarContextMenuModel>(
      delegate(), Type::kExpandedBarMenu);
  AddItemWithIcon(base::to_underlying(CommandId::kHideSuggestion),
                  u"Hide this suggestion",
                  CreateIcon(kSystemTrayDoNotDisturbIcon));
  AddItemWithIcon(base::to_underlying(CommandId::kHideDriveSuggestions),
                  u"Hide all Google Drive suggestions",
                  CreateIcon(kForbidIcon));
  AddSubMenuWithIcon(base::to_underlying(CommandId::kCustomizeSuggestions),
                     u"Customize suggestions", sub_menu_model_.get(),
                     CreateIcon(kPencilIcon));
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithIcon(base::to_underlying(CommandId::kFeedback), u"Send Feedback",
                  CreateIcon(kFeedbackIcon));
}

}  // namespace ash
