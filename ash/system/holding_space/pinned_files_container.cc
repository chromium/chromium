// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_container.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/containers/adapters.h"
#include "base/optional.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

bool HasEverPinnedHoldingSpaceItem() {
  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return active_pref_service
             ? holding_space_prefs::GetTimeOfFirstPin(active_pref_service)
                   .has_value()
             : false;
}

// HoldingSpaceScrollView ------------------------------------------------------

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

  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    // Sync scroll view visibility with contents visibility.
    if (GetVisible() != observed_view->GetVisible())
      SetVisible(observed_view->GetVisible());
  }

  void OnViewIsDeleting(View* observed_view) override {
    view_observer_.Remove(observed_view);
  }

 private:
  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};

}  // namespace

// PinnedFilesContainer --------------------------------------------------------

PinnedFilesContainer::PinnedFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(kHoldingSpacePinnedFilesContainerId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

  auto* title_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_TITLE)));
  title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label->SetPaintToLayer();
  title_label->layer()->SetFillsBoundsOpaquely(false);

  auto label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_label->SetEnabledColor(label_color);
  TrayPopupUtils::SetLabelFontList(title_label,
                                   TrayPopupUtils::FontStyle::kSubHeader);

  if (!HasEverPinnedHoldingSpaceItem()) {
    empty_prompt_label_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT)));
    empty_prompt_label_->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    empty_prompt_label_->SetMultiLine(true);
    empty_prompt_label_->SetPaintToLayer();
    empty_prompt_label_->layer()->SetFillsBoundsOpaquely(false);
    empty_prompt_label_->SetEnabledColor(label_color);
    TrayPopupUtils::SetLabelFontList(
        empty_prompt_label_, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  }

  auto* scroll_view = AddChildView(std::make_unique<HoldingSpaceScrollView>());
  scroll_view->SetBackgroundColor(base::nullopt);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetVisible(false);
  scroll_view->SetPaintToLayer();
  scroll_view->layer()->SetFillsBoundsOpaquely(false);
  scroll_view->ClipHeightTo(0, INT_MAX);

  item_chips_container_ = scroll_view->SetContents(
      std::make_unique<HoldingSpaceItemChipsContainer>());

  if (HasEverPinnedHoldingSpaceItem())
    SetVisible(false);
  else
    item_chips_container_->SetVisible(false);
}

PinnedFilesContainer::~PinnedFilesContainer() = default;

void PinnedFilesContainer::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // We only care about `item_chips_container_` becoming empty and non-empty.
  if (details.parent != item_chips_container_ ||
      item_chips_container_->children().size() != 1u) {
    return;
  }

  if (details.is_add && empty_prompt_label_ &&
      empty_prompt_label_->GetVisible()) {
    // An item has been added to `item_chips_container_` for the first time.
    empty_prompt_label_->SetVisible(false);
    item_chips_container_->SetVisible(true);
  }

  SetVisible(details.is_add);
}

void PinnedFilesContainer::AddHoldingSpaceItemView(const HoldingSpaceItem* item,
                                                   bool due_to_finalization) {
  DCHECK(!base::Contains(views_by_item_id_, item->id()));
  DCHECK(item->IsFinalized());

  if (item->type() != HoldingSpaceItem::Type::kPinnedFile)
    return;

  size_t index = 0;

  if (due_to_finalization) {
    // Find the position at which the view should be added.
    for (const auto& candidate :
         base::Reversed(HoldingSpaceController::Get()->model()->items())) {
      if (candidate->id() == item->id())
        break;
      if (candidate->IsFinalized() &&
          candidate->type() == HoldingSpaceItem::Type::kPinnedFile) {
        ++index;
      }
    }
  }

  views_by_item_id_[item->id()] = item_chips_container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemChipView>(delegate_, item), index);
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
