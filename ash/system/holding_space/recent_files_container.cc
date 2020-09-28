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
#include "base/containers/adapters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

RecentFilesContainer::RecentFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(kHoldingSpaceRecentFilesContainerId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

  auto setup_label = [](views::Label* label) {
    TrayPopupItemStyle(TrayPopupItemStyle::FontStyle::SUB_HEADER)
        .SetupLabel(label);

    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    label->SetPaintToLayer();
    label->layer()->SetFillsBoundsOpaquely(false);
  };

  auto* screenshots_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREENSHOTS_TITLE)));
  setup_label(screenshots_label);

  screenshots_container_ = AddChildView(std::make_unique<views::View>());
  screenshots_container_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kHoldingSpaceScreenshotsContainerPadding)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(/*top=*/0, /*left=*/0, /*bottom=*/0,
                              /*right=*/kHoldingSpaceScreenshotSpacing));

  auto* downloads_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)));
  setup_label(downloads_label);

  downloads_container_ =
      AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());

  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

RecentFilesContainer::~RecentFilesContainer() = default;

void RecentFilesContainer::AddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  if (item->type() == HoldingSpaceItem::Type::kScreenshot)
    AddHoldingSpaceScreenshotView(item);
  else if (item->type() == HoldingSpaceItem::Type::kDownload)
    AddHoldingSpaceDownloadView(item);
}

void RecentFilesContainer::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  screenshots_container_->RemoveAllChildViews(true);
  downloads_container_->RemoveAllChildViews(true);
}

void RecentFilesContainer::RemoveHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  if (item->type() == HoldingSpaceItem::Type::kScreenshot)
    RemoveHoldingSpaceScreenshotView(item);
  else if (item->type() == HoldingSpaceItem::Type::kDownload)
    RemoveHoldingSpaceDownloadView(item);
}

void RecentFilesContainer::AddHoldingSpaceScreenshotView(
    const HoldingSpaceItem* item) {
  DCHECK_EQ(item->type(), HoldingSpaceItem::Type::kScreenshot);
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  // Remove the last screenshot view if we are already at max capacity.
  if (screenshots_container_->children().size() == kMaxScreenshots) {
    std::unique_ptr<views::View> view =
        screenshots_container_->RemoveChildViewT(
            screenshots_container_->children().back());
    views_by_item_id_.erase(
        HoldingSpaceItemView::Cast(view.get())->item()->id());
  }

  // Add the screenshot view to the front in order to sort by recency.
  views_by_item_id_[item->id()] = screenshots_container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemScreenshotView>(delegate_, item),
      /*index=*/0);
}

void RecentFilesContainer::RemoveHoldingSpaceScreenshotView(
    const HoldingSpaceItem* item) {
  DCHECK_EQ(item->type(), HoldingSpaceItem::Type::kScreenshot);

  auto it = views_by_item_id_.find(item->id());
  if (it == views_by_item_id_.end())
    return;

  // Remove the screenshot view associated with `item`.
  screenshots_container_->RemoveChildViewT(it->second);
  views_by_item_id_.erase(it);

  // Verify that we are *not* at max capacity.
  DCHECK_LT(screenshots_container_->children().size(), kMaxScreenshots);

  // Since we are under max capacity, we can add at most one screenshot view to
  // replace the view we just removed. Note that we add the replacement to the
  // back in order to maintain sort by recency.
  for (const auto& candidate :
       base::Reversed(HoldingSpaceController::Get()->model()->items())) {
    if (candidate->type() == HoldingSpaceItem::Type::kScreenshot &&
        !base::Contains(views_by_item_id_, candidate->id())) {
      views_by_item_id_[candidate->id()] = screenshots_container_->AddChildView(
          std::make_unique<HoldingSpaceItemScreenshotView>(delegate_,
                                                           candidate.get()));
      return;
    }
  }
}

void RecentFilesContainer::AddHoldingSpaceDownloadView(
    const HoldingSpaceItem* item) {
  DCHECK_EQ(item->type(), HoldingSpaceItem::Type::kDownload);
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  // Remove the last download view if we are already at max capacity.
  if (downloads_container_->children().size() == kMaxDownloads) {
    std::unique_ptr<views::View> view = downloads_container_->RemoveChildViewT(
        downloads_container_->children().back());
    views_by_item_id_.erase(
        HoldingSpaceItemView::Cast(view.get())->item()->id());
  }

  // Add the download view to the front in order to sort by recency.
  views_by_item_id_[item->id()] = downloads_container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemChipView>(delegate_, item), /*index=*/0);
}

void RecentFilesContainer::RemoveHoldingSpaceDownloadView(
    const HoldingSpaceItem* item) {
  DCHECK_EQ(item->type(), HoldingSpaceItem::Type::kDownload);

  auto it = views_by_item_id_.find(item->id());
  if (it == views_by_item_id_.end())
    return;

  // Remove the download view associated with `item`.
  downloads_container_->RemoveChildViewT(it->second);
  views_by_item_id_.erase(it);

  // Verify that we are *not* at max capacity.
  DCHECK_LT(downloads_container_->children().size(), kMaxDownloads);

  // Since we are under max capacity, we can add at most one download view to
  // replace the view we just removed. Note that we add the replacement to the
  // back in order to maintain sort by recency.
  for (const auto& candidate :
       base::Reversed(HoldingSpaceController::Get()->model()->items())) {
    if (candidate->type() == HoldingSpaceItem::Type::kDownload &&
        !base::Contains(views_by_item_id_, candidate->id())) {
      views_by_item_id_[candidate->id()] = downloads_container_->AddChildView(
          std::make_unique<HoldingSpaceItemChipView>(delegate_,
                                                     candidate.get()));
      return;
    }
  }
}

}  // namespace ash