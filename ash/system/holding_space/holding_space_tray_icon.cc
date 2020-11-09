// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_icon_preview.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/stl_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

HoldingSpaceTrayIcon::HoldingSpaceTrayIcon(Shelf* shelf) : shelf_(shelf) {
  InitLayout();

  if (features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled()) {
    controller_observer_.Add(HoldingSpaceController::Get());
    shell_observer_.Add(Shell::Get());
  }
}

HoldingSpaceTrayIcon::~HoldingSpaceTrayIcon() = default;

int HoldingSpaceTrayIcon::GetPreferredMainAxisMargin() const {
  return features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled()
             ? 0
             : kHoldingSpaceTrayIconMainAxisMargin;
}

void HoldingSpaceTrayIcon::OnLocaleChanged() {
  TooltipTextChanged();
}

base::string16 HoldingSpaceTrayIcon::GetTooltipText(
    const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

void HoldingSpaceTrayIcon::InitLayout() {
  if (features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled()) {
    SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

    // As holding space items are added to the model, child layers will be added
    // to `this` view's layer to represent them.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    return;
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Image.
  auto* image_view = AddChildView(std::make_unique<views::ImageView>());
  image_view->SetImage(
      gfx::CreateVectorIcon(kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
                            ShelfConfig::Get()->shelf_icon_color()));

  // Disallow events on `image_view` so that tooltips will be retrieved from
  // `this`. Moving forward, `image_view` will not exist as we transition to a
  // more content forward tray icon.
  image_view->SetCanProcessEventsWithinSubtree(false);
}

void HoldingSpaceTrayIcon::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  model_observer_.Add(model);
  for (const std::unique_ptr<HoldingSpaceItem>& item : model->items())
    OnHoldingSpaceItemAdded(item.get());
}

void HoldingSpaceTrayIcon::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  model_observer_.Remove(model);
  for (const std::unique_ptr<HoldingSpaceItem>& item : model->items())
    OnHoldingSpaceItemRemoved(item.get());
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  if (!item->IsFinalized())
    return;

  for (std::unique_ptr<HoldingSpaceTrayIconPreview>& preview : previews_)
    preview->AnimateShift();

  auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(this, item);
  previews_.push_back(std::move(preview));
  previews_.back()->AnimateIn(/*index=*/0u);

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  if (!item->IsFinalized())
    return;

  size_t index = previews_.size();
  for (size_t i = 0u; i < previews_.size(); ++i) {
    if (previews_[i]->item() == item) {
      index = i;
      break;
    }
  }

  DCHECK_LT(index, previews_.size());

  removed_previews_.push_back(std::move(previews_[index]));
  previews_.erase(previews_.begin() + index);

  for (int i = index - 1; i >= 0; --i)
    previews_[i]->AnimateUnshift();

  removed_previews_.back()->AnimateOut(base::BindOnce(
      &HoldingSpaceTrayIcon::OnHoldingSpaceTrayIconPreviewAnimatedOut,
      base::Unretained(this),
      base::Unretained(removed_previews_.back().get())));

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  if (previews_.empty()) {
    OnHoldingSpaceItemAdded(item);
    return;
  }

  size_t index = 0;
  for (const auto& candidate :
       HoldingSpaceController::Get()->model()->items()) {
    if (candidate->id() == item->id())
      break;
    if (candidate->IsFinalized())
      ++index;
  }

  for (size_t i = 0; i < index; ++i)
    previews_[i]->AnimateShift();

  auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(this, item);
  previews_.insert(previews_.begin() + index, std::move(preview));

  // NOTE: UI sort is inverse of model sort so take `index` from end.
  previews_[index]->AnimateIn(previews_.size() - index - 1);

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  DCHECK(features::IsTemporaryHoldingSpaceContentForwardEntryPointEnabled());

  removed_previews_.clear();

  for (const auto& preview : previews_)
    preview->OnShelfAlignmentChanged(old_alignment, shelf_->alignment());

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::UpdatePreferredSize() {
  const int num_visible_previews =
      std::min(kHoldingSpaceTrayIconMaxVisiblePreviews,
               static_cast<int>(previews_.size()));

  int primary_axis_size = kTrayItemSize;
  if (num_visible_previews > 1)
    primary_axis_size += (num_visible_previews - 1) * kTrayItemSize / 2;

  gfx::Size preferred_size = shelf_->PrimaryAxisValue(
      /*horizontal=*/gfx::Size(primary_axis_size, kTrayItemSize),
      /*vertical=*/gfx::Size(kTrayItemSize, primary_axis_size));

  if (preferred_size != GetPreferredSize())
    SetPreferredSize(preferred_size);
}

void HoldingSpaceTrayIcon::OnHoldingSpaceTrayIconPreviewAnimatedOut(
    HoldingSpaceTrayIconPreview* preview) {
  base::EraseIf(removed_previews_, base::MatchesUniquePtr(preview));
}

BEGIN_METADATA(HoldingSpaceTrayIcon, views::View)
END_METADATA

}  // namespace ash
