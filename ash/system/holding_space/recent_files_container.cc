// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_container.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/holding_space/holding_space_item_screenshot_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

RecentFilesContainer::RecentFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(kHoldingSpaceRecentFilesContainerId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

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
      views::BoxLayout::Orientation::kHorizontal,
      kHoldingSpaceScreenshotsContainerPadding,
      kHoldingSpaceScreenshotSpacing));

  auto* recent_downloads_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_RECENT_DOWNLOADS_TITLE)));
  setup_layered_child(recent_downloads_label);
  style.SetupLabel(recent_downloads_label);

  recent_downloads_container_ =
      AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());

  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

RecentFilesContainer::~RecentFilesContainer() = default;

void RecentFilesContainer::AddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  DCHECK(!base::Contains(views_by_item_id_, item->id()));
  if (item->type() == HoldingSpaceItem::Type::kScreenshot) {
    views_by_item_id_[item->id()] = screenshots_container_->AddChildViewAt(
        std::make_unique<HoldingSpaceItemScreenshotView>(delegate_, item),
        /*index=*/0);
  } else if (item->type() == HoldingSpaceItem::Type::kDownload) {
    views_by_item_id_[item->id()] = recent_downloads_container_->AddChildViewAt(
        std::make_unique<HoldingSpaceItemChipView>(delegate_, item),
        /*index=*/0);
  }
}

void RecentFilesContainer::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  screenshots_container_->RemoveAllChildViews(true);
  recent_downloads_container_->RemoveAllChildViews(true);
}

void RecentFilesContainer::RemoveHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  auto it = views_by_item_id_.find(item->id());
  if (it == views_by_item_id_.end())
    return;

  if (item->type() == HoldingSpaceItem::Type::kScreenshot) {
    screenshots_container_->RemoveChildViewT(it->second);
  } else if (item->type() == HoldingSpaceItem::Type::kDownload) {
    recent_downloads_container_->RemoveChildViewT(it->second);
  }

  views_by_item_id_.erase(it);
}

}  // namespace ash