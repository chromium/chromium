// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/downloads_section.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns if an item of the specified `type` belongs in this section.
bool BelongsToSection(HoldingSpaceItem::Type type) {
  return type == HoldingSpaceItem::Type::kDownload ||
         type == HoldingSpaceItem::Type::kNearbyShare;
}

// Header-----------------------------------------------------------------------

class Header : public views::Button {
 public:
  Header() {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE));
    SetCallback(
        base::BindRepeating(&Header::OnPressed, base::Unretained(this)));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHoldingSpaceDownloadsHeaderSpacing));

    // Label.
    auto* label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)));
    label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    layout->SetFlexForView(label, 1);

    TrayPopupUtils::SetLabelFontList(label,
                                     TrayPopupUtils::FontStyle::kSubHeader);

    // Chevron.
    auto* chevron = AddChildView(std::make_unique<views::ImageView>());
    chevron->SetFlipCanvasOnPaintForRTLUI(true);
    chevron->SetImage(gfx::CreateVectorIcon(
        kChevronRightIcon, kHoldingSpaceDownloadsChevronIconSize,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  }

 private:
  void OnPressed() {
    holding_space_metrics::RecordDownloadsAction(
        holding_space_metrics::DownloadsAction::kClick);

    HoldingSpaceController::Get()->client()->OpenDownloads(base::DoNothing());
  }
};

}  // namespace

// DownloadsSection ------------------------------------------------------------

DownloadsSection::DownloadsSection(HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsContainer(delegate) {
  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHoldingSpaceContainerChildSpacing));

  // Header.
  header_ = AddChildView(std::make_unique<Header>());
  header_->SetPaintToLayer();
  header_->layer()->SetFillsBoundsOpaquely(false);
  header_->SetVisible(false);

  // Container.
  container_ = AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());
  container_->SetVisible(false);
}

DownloadsSection::~DownloadsSection() = default;

void DownloadsSection::ChildVisibilityChanged(views::View* child) {
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

void DownloadsSection::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != container_)
    return;

  // Update visibility when becoming empty or non-empty. Note that in the case
  // of a view being added, `ViewHierarchyChanged()` is called *after* the view
  // has been parented but in the case of a view being removed, it is called
  // *before* the view is unparented.
  if (container_->children().size() == 1u) {
    header_->SetVisible(details.is_add);
    container_->SetVisible(details.is_add);
  }
}

bool DownloadsSection::ContainsHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return base::Contains(views_by_item_id_, item->id());
}

bool DownloadsSection::ContainsHoldingSpaceItemViews() {
  return !views_by_item_id_.empty();
}

bool DownloadsSection::WillAddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return BelongsToSection(item->type());
}

void DownloadsSection::AddHoldingSpaceItemView(const HoldingSpaceItem* item) {
  DCHECK(item->IsFinalized());
  DCHECK(BelongsToSection(item->type()));
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  // Remove the last download view if we are already at max capacity.
  if (container_->children().size() == kMaxDownloads) {
    std::unique_ptr<views::View> view =
        container_->RemoveChildViewT(container_->children().back());
    views_by_item_id_.erase(
        HoldingSpaceItemView::Cast(view.get())->item()->id());
  }

  // Add the download view to the front in order to sort by recency.
  views_by_item_id_[item->id()] = container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemChipView>(delegate(), item), 0);
}

void DownloadsSection::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  container_->RemoveAllChildViews(true);
}

// TODO(dmblack): Handle grow/shrink of container.
void DownloadsSection::AnimateIn(ui::LayerAnimationObserver* observer) {
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateIn(observer);
}

// TODO(dmblack): Handle animate out of `header_` if this section is going away
// permanently.
void DownloadsSection::AnimateOut(ui::LayerAnimationObserver* observer) {
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateOut(observer);
}

}  // namespace ash
