// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_container.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "base/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {
class HoldingSpaceScrollView : public views::ScrollView,
                               public views::ViewObserver {
 public:
  HoldingSpaceItemChipsContainer* SetContents(
      std::unique_ptr<HoldingSpaceItemChipsContainer> view) {
    HoldingSpaceItemChipsContainer* contents =
        views::ScrollView::SetContents(std::move(view));
    view_observer_.Add(contents);
    return contents;
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(View* observed_view) override {
    PreferredSizeChanged();
  }

  void OnViewIsDeleting(View* observed_view) override {
    view_observer_.Remove(observed_view);
  }

 private:
  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};
}  // namespace

PinnedFilesContainer::PinnedFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(kHoldingSpacePinnedFilesContainerId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

  auto* title_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_TITLE)));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::HOLDING_SPACE_TITLE,
                           true /* use_unified_theme */);
  style.SetupLabel(title_label);
  title_label->SetPaintToLayer();
  title_label->layer()->SetFillsBoundsOpaquely(false);

  auto* scroller = AddChildView(std::make_unique<HoldingSpaceScrollView>());
  scroller->SetBackgroundColor(base::nullopt);
  scroller->SetDrawOverflowIndicator(false);
  scroller->ClipHeightTo(0, INT_MAX);

  item_chips_container_ =
      scroller->SetContents(std::make_unique<HoldingSpaceItemChipsContainer>());

  if (HoldingSpaceController::Get()->model())
    OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
}

PinnedFilesContainer::~PinnedFilesContainer() = default;

void PinnedFilesContainer::AddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  if (item->type() == HoldingSpaceItem::Type::kPinnedFile) {
    views_by_item_id_[item->id()] = item_chips_container_->AddChildViewAt(
        std::make_unique<HoldingSpaceItemChipView>(delegate_, item),
        /*index=*/0);
  }
}

void PinnedFilesContainer::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  item_chips_container_->RemoveAllChildViews(true);
}

void PinnedFilesContainer::RemoveHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  auto it = views_by_item_id_.find(item->id());
  if (it == views_by_item_id_.end())
    return;

  item_chips_container_->RemoveChildViewT(it->second);
  views_by_item_id_.erase(it->first);
}

}  // namespace ash
