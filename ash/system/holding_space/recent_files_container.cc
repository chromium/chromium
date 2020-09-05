// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_container.h"

#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/holding_space/holding_space_screenshot_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

RecentFilesContainer::RecentFilesContainer() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding));
  auto setup_layered_child = [](views::View* child) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
  };

  auto* screenshots_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE)));
  setup_layered_child(screenshots_label);
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::HOLDING_SPACE_TITLE,
                           true /* use_unified_theme */);
  style.SetupLabel(screenshots_label);

  screenshots_container_ = AddChildView(std::make_unique<views::View>());
  screenshots_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(16, 0, 24, 0),
      8));

  // TODO(crbug.com/1125254): Populate containers if and when holding space
  // model is attached, below is a temporary solution.
  for (const auto& item : HoldingSpaceController::Get()->model()->items()) {
    if (item->type() == HoldingSpaceItem::Type::kScreenshot) {
      screenshots_container_->AddChildView(
          std::make_unique<HoldingSpaceScreenshotView>(item.get()));
    }
  }

  auto* recent_downloads_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_RECENT_DOWNLOADS_TITLE)));
  setup_layered_child(recent_downloads_label);
  style.SetupLabel(recent_downloads_label);

  recent_downloads_container_ =
      AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());
  // TODO(crbug.com/1125254): Populate containers if and when holding space
  // model is attached, below is a temporary solution.
  for (const auto& item : HoldingSpaceController::Get()->model()->items()) {
    if (item->type() == HoldingSpaceItem::Type::kDownload)
      recent_downloads_container_->AddItemChip(item.get());
  }
}

RecentFilesContainer::~RecentFilesContainer() = default;

const char* RecentFilesContainer::GetClassName() const {
  return "RecentFilesContainer";
}

}  // namespace ash