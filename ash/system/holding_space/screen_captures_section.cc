// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/screen_captures_section.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns if an item of the specified `type` belongs in this section.
bool BelongsToSection(HoldingSpaceItem::Type type) {
  return type == HoldingSpaceItem::Type::kScreenshot ||
         type == HoldingSpaceItem::Type::kScreenRecording;
}

}  // namespace

// ScreenCapturesSection -------------------------------------------------------

ScreenCapturesSection::ScreenCapturesSection(
    HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsContainer(delegate) {
  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHoldingSpaceContainerChildSpacing));

  // Label.
  label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREEN_CAPTURES_TITLE)));
  label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetVisible(false);

  TrayPopupUtils::SetLabelFontList(label_,
                                   TrayPopupUtils::FontStyle::kSubHeader);

  // Container.
  container_ = AddChildView(std::make_unique<views::View>());
  container_->SetVisible(false);
  container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(/*top=*/0, /*left=*/0, /*bottom=*/0,
                              /*right=*/kHoldingSpaceScreenCaptureSpacing));
}

ScreenCapturesSection::~ScreenCapturesSection() = default;

void ScreenCapturesSection::ChildVisibilityChanged(views::View* child) {
  // This section should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children()) {
    if (c->GetVisible()) {
      visible = true;
      break;
    }
  }

  if (visible != GetVisible())
    SetVisible(visible);

  HoldingSpaceItemViewsContainer::ChildVisibilityChanged(child);
}

void ScreenCapturesSection::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != container_)
    return;

  // Update visibility when becoming empty or non-empty. Note that in the case
  // of a view being added, `ViewHierarchyChanged()` is called *after* the view
  // has been parented but in the case of a view being removed, it is called
  // *before* the view is unparented.
  if (container_->children().size() == 1u) {
    label_->SetVisible(details.is_add);
    container_->SetVisible(details.is_add);
  }
}

bool ScreenCapturesSection::ContainsHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return base::Contains(views_by_item_id_, item->id());
}

bool ScreenCapturesSection::ContainsHoldingSpaceItemViews() {
  return !views_by_item_id_.empty();
}

bool ScreenCapturesSection::WillAddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return BelongsToSection(item->type());
}

void ScreenCapturesSection::AddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  DCHECK(item->IsFinalized());
  DCHECK(BelongsToSection(item->type()));
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  // Remove the last screen capture view if we are already at max capacity.
  if (container_->children().size() == kMaxScreenCaptures) {
    std::unique_ptr<views::View> view =
        container_->RemoveChildViewT(container_->children().back());
    views_by_item_id_.erase(
        HoldingSpaceItemView::Cast(view.get())->item()->id());
  }

  // Add the screen capture view to the front in order to sort by recency.
  views_by_item_id_[item->id()] = container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemScreenCaptureView>(delegate(), item), 0);
}

void ScreenCapturesSection::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  container_->RemoveAllChildViews(true);
}

// TODO(dmblack): Handle grow/shrink of container.
void ScreenCapturesSection::AnimateIn(ui::LayerAnimationObserver* observer) {
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateIn(observer);
}

// TODO(dmblack): Handle animate out of `label_` if this section is going away
// permanently.
void ScreenCapturesSection::AnimateOut(ui::LayerAnimationObserver* observer) {
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateOut(observer);
}

}  // namespace ash
