// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/recent_files_container.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/containers/adapters.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Sets up the specified `label`.
void SetupLabel(views::Label* label) {
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(label,
                                   TrayPopupUtils::FontStyle::kSubHeader);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

// Returns if an item of the specified `type` belongs in the downloads section.
bool BelongsToDownloadsSection(HoldingSpaceItem::Type type) {
  return type == HoldingSpaceItem::Type::kDownload ||
         type == HoldingSpaceItem::Type::kNearbyShare;
}

// Returns if an item of the specified `type` belongs in the screen captures
// section.
bool BelongsToScreenCaptureSection(HoldingSpaceItem::Type type) {
  return type == HoldingSpaceItem::Type::kScreenshot ||
         type == HoldingSpaceItem::Type::kScreenRecording;
}

// DownloadsHeader--------------------------------------------------------------

class DownloadsHeader : public views::Button {
 public:
  explicit DownloadsHeader() {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHoldingSpaceDownloadsHeaderSpacing));

    auto* label = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)));
    layout->SetFlexForView(label, 1);
    SetupLabel(label);

    auto* chevron = AddChildView(std::make_unique<views::ImageView>());
    chevron->SetFlipCanvasOnPaintForRTLUI(true);

    const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);
    chevron->SetImage(CreateVectorIcon(
        kChevronRightIcon, kHoldingSpaceDownloadsChevronIconSize, icon_color));

    SetCallback(base::BindRepeating(&DownloadsHeader::OnPressed,
                                    base::Unretained(this)));
  }

 private:
  void OnPressed() {
    holding_space_metrics::RecordDownloadsAction(
        holding_space_metrics::DownloadsAction::kClick);

    HoldingSpaceController::Get()->client()->OpenDownloads(base::DoNothing());
  }
};

}  // namespace

// RecentFilesContainer --------------------------------------------------------

RecentFilesContainer::RecentFilesContainer(
    HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsContainer(delegate) {
  SetID(kHoldingSpaceRecentFilesContainerId);
  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHoldingSpaceContainerPadding,
      kHoldingSpaceContainerChildSpacing));

  screen_captures_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SCREEN_CAPTURES_TITLE)));
  screen_captures_label_->SetPaintToLayer();
  screen_captures_label_->layer()->SetFillsBoundsOpaquely(false);
  screen_captures_label_->SetVisible(false);
  SetupLabel(screen_captures_label_);

  screen_captures_container_ = AddChildView(std::make_unique<views::View>());
  screen_captures_container_->SetVisible(false);
  screen_captures_container_
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(/*top=*/0, /*left=*/0, /*bottom=*/0,
                              /*right=*/kHoldingSpaceScreenCaptureSpacing));

  downloads_header_ = AddChildView(std::make_unique<DownloadsHeader>());
  downloads_header_->SetPaintToLayer();
  downloads_header_->layer()->SetFillsBoundsOpaquely(false);
  downloads_header_->SetVisible(false);

  downloads_container_ =
      AddChildView(std::make_unique<HoldingSpaceItemChipsContainer>());
  downloads_container_->SetVisible(false);
}

RecentFilesContainer::~RecentFilesContainer() = default;

void RecentFilesContainer::ChildVisibilityChanged(views::View* child) {
  // The recent files container should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children())
    visible |= c->GetVisible();

  if (visible != GetVisible())
    SetVisible(visible);

  HoldingSpaceItemViewsContainer::ChildVisibilityChanged(child);
}

void RecentFilesContainer::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent == screen_captures_container_)
    OnScreenCapturesContainerViewHierarchyChanged(details);
  else if (details.parent == downloads_container_)
    OnDownloadsContainerViewHierarchyChanged(details);
}

bool RecentFilesContainer::ContainsHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return base::Contains(views_by_item_id_, item->id());
}

bool RecentFilesContainer::ContainsHoldingSpaceItemViews() {
  return !views_by_item_id_.empty();
}

bool RecentFilesContainer::WillAddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  return BelongsToDownloadsSection(item->type()) ||
         BelongsToScreenCaptureSection(item->type());
}

void RecentFilesContainer::AddHoldingSpaceItemView(
    const HoldingSpaceItem* item) {
  DCHECK(item->IsFinalized());

  if (BelongsToScreenCaptureSection(item->type()))
    AddHoldingSpaceScreenCaptureView(item);
  else if (BelongsToDownloadsSection(item->type()))
    AddHoldingSpaceDownloadView(item);
  else
    NOTREACHED();
}

void RecentFilesContainer::RemoveAllHoldingSpaceItemViews() {
  views_by_item_id_.clear();
  screen_captures_container_->RemoveAllChildViews(true);
  downloads_container_->RemoveAllChildViews(true);
}

// TODO(dmblack): Implement.
void RecentFilesContainer::AnimateIn(ui::ImplicitAnimationObserver* observer) {
  NOTIMPLEMENTED();
}

// TODO(dmblack): Implement.
void RecentFilesContainer::AnimateOut(ui::ImplicitAnimationObserver* observer) {
  NOTIMPLEMENTED();
}

void RecentFilesContainer::AddHoldingSpaceScreenCaptureView(
    const HoldingSpaceItem* item) {
  DCHECK(BelongsToScreenCaptureSection(item->type()));
  DCHECK(!base::Contains(views_by_item_id_, item->id()));

  // Remove the last screen capture view if we are already at max capacity.
  if (screen_captures_container_->children().size() == kMaxScreenCaptures) {
    std::unique_ptr<views::View> view =
        screen_captures_container_->RemoveChildViewT(
            screen_captures_container_->children().back());
    views_by_item_id_.erase(
        HoldingSpaceItemView::Cast(view.get())->item()->id());
  }

  // Add the screen capture view to the front in order to sort by recency.
  views_by_item_id_[item->id()] = screen_captures_container_->AddChildViewAt(
      std::make_unique<HoldingSpaceItemScreenCaptureView>(delegate(), item), 0);
}

void RecentFilesContainer::AddHoldingSpaceDownloadView(
    const HoldingSpaceItem* item) {
  DCHECK(BelongsToDownloadsSection(item->type()));
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
      std::make_unique<HoldingSpaceItemChipView>(delegate(), item), 0);
}

void RecentFilesContainer::OnScreenCapturesContainerViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // Update screen capture visibility when becoming empty or non-empty.
  if (screen_captures_container_->children().size() == 1u) {
    screen_captures_label_->SetVisible(details.is_add);
    screen_captures_container_->SetVisible(details.is_add);
  }
}

void RecentFilesContainer::OnDownloadsContainerViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  // Update downloads visibility when becoming empty or non-empty.
  if (downloads_container_->children().size() == 1u) {
    downloads_header_->SetVisible(details.is_add);
    downloads_container_->SetVisible(details.is_add);
  }
}

}  // namespace ash
