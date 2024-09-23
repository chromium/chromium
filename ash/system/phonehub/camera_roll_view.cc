// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/camera_roll_thumbnail.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance constants in dip.
constexpr int kCameraRollItemsInRow = 4;
constexpr int kCameraRollItemHorizontalSpacing = 8;
constexpr int kCameraRollItemVerticalSpacing = 8;
constexpr int kCameraRollItemHorizontalPadding = 4;
constexpr int kCameraRollItemVerticalPadding = 4;
constexpr int kHeaderLabelLineHeight = 48;

gfx::Size GetCameraRollItemSize() {
  int dimension =
      (kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2 -
       kCameraRollItemHorizontalPadding * 2 -
       kCameraRollItemHorizontalSpacing * (kCameraRollItemsInRow - 1)) /
      kCameraRollItemsInRow;
  return gfx::Size(dimension, dimension);
}

class HeaderView : public views::Label {
  METADATA_HEADER(HeaderView, views::Label)

 public:
  HeaderView() {
    SetText(l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_CAMERA_ROLL_TITLE));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));

    TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                          *this);

    // Overriding because the typography line height set does not match Phone
    // Hub specs.
    SetLineHeight(kHeaderLabelLineHeight);
  }

  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;

};

BEGIN_METADATA(HeaderView)
END_METADATA

}  // namespace

CameraRollView::CameraRollView(
    phonehub::CameraRollManager* camera_roll_manager,
    phonehub::UserActionRecorder* user_action_recorder)
    : camera_roll_manager_(camera_roll_manager),
      user_action_recorder_(user_action_recorder) {
  SetID(PhoneHubViewID::kCameraRollView);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<HeaderView>());
  items_view_ = AddChildView(std::make_unique<CameraRollItemsView>());

  Update();
  camera_roll_manager_->AddObserver(this);
}

CameraRollView::~CameraRollView() {
  camera_roll_manager_->RemoveObserver(this);
}

void CameraRollView::OnCameraRollViewUiStateUpdated() {
  Update();
}

CameraRollView::CameraRollItemsView::CameraRollItemsView() = default;

CameraRollView::CameraRollItemsView::~CameraRollItemsView() = default;

void CameraRollView::CameraRollItemsView::AddCameraRollItem(
    views::View* camera_roll_item) {
  size_t view_size = camera_roll_items_.view_size();
  camera_roll_items_.Add(camera_roll_item, view_size);
  AddChildView(camera_roll_item);
}

void CameraRollView::CameraRollItemsView::Reset() {
  camera_roll_items_.Clear();
  RemoveAllChildViews();
}

// views::View:
gfx::Size CameraRollView::CameraRollItemsView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto item_size = GetCameraRollItemSize();
  int width = item_size.width() * kCameraRollItemsInRow +
              kCameraRollItemHorizontalSpacing * (kCameraRollItemsInRow - 1) +
              kCameraRollItemHorizontalPadding * 2;
  int rows_num =
      std::ceil((double)camera_roll_items_.view_size() / kCameraRollItemsInRow);
  int height = (item_size.height() + kCameraRollItemVerticalSpacing) *
                   std::max(0, rows_num - 1) +
               item_size.height() + kCameraRollItemVerticalPadding * 2;
  return gfx::Size(width, height);
}

void CameraRollView::CameraRollItemsView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  CalculateIdealBounds();
  for (size_t i = 0; i < camera_roll_items_.view_size(); ++i) {
    auto* thumbnail = camera_roll_items_.view_at(i);
    thumbnail->SetBoundsRect(camera_roll_items_.ideal_bounds(i));
  }
}

gfx::Point CameraRollView::CameraRollItemsView::GetCameraRollItemPosition(
    int index) {
  auto item_size = GetCameraRollItemSize();
  int row = index / kCameraRollItemsInRow;
  int column = index % kCameraRollItemsInRow;
  int x = (item_size.width() + kCameraRollItemHorizontalSpacing) * column +
          kCameraRollItemHorizontalPadding;
  int y = (item_size.height() + kCameraRollItemVerticalSpacing) * row +
          kCameraRollItemVerticalPadding;
  return gfx::Point(x, y);
}

void CameraRollView::CameraRollItemsView::CalculateIdealBounds() {
  for (size_t i = 0; i < camera_roll_items_.view_size(); ++i) {
    gfx::Rect camera_roll_item_bounds =
        gfx::Rect(GetCameraRollItemPosition(i), GetCameraRollItemSize());
    camera_roll_items_.set_ideal_bounds(i, camera_roll_item_bounds);
  }
}

BEGIN_METADATA(CameraRollView, CameraRollItemsView)
END_METADATA

void CameraRollView::Update() {
  items_view_->Reset();
  phonehub::CameraRollManager::CameraRollUiState current_ui_state =
      camera_roll_manager_->ui_state();

  switch (current_ui_state) {
    case phonehub::CameraRollManager::CameraRollUiState::SHOULD_HIDE:
    case phonehub::CameraRollManager::CameraRollUiState::NO_STORAGE_PERMISSION:
      SetVisible(false);
      break;
    case phonehub::CameraRollManager::CameraRollUiState::ITEMS_VISIBLE:
      items_view_->SetVisible(true);
      SetVisible(true);
      const std::vector<phonehub::CameraRollItem> camera_roll_items =
          camera_roll_manager_->current_items();
      for (size_t index = 0; index < camera_roll_items.size(); index++) {
        CameraRollThumbnail* item_thumbnail = new CameraRollThumbnail(
            index, camera_roll_items.at(index), camera_roll_manager_,
            user_action_recorder_);

        const std::u16string accessible_name = l10n_util::GetStringFUTF16(
            IDS_ASH_PHONE_HUB_CAMERA_ROLL_THUMBNAIL_ACCESSIBLE_NAME,
            base::NumberToString16(index + 1),
            base::NumberToString16(camera_roll_items.size()));
        item_thumbnail->GetViewAccessibility().SetName(accessible_name);
        item_thumbnail->SetTooltipText(accessible_name);
        items_view_->AddCameraRollItem(item_thumbnail);
      }
      if (!content_present_metric_emitted_) {
        phone_hub_metrics::LogCameraRollContentPresent();
        content_present_metric_emitted_ = true;
      }
      break;
  }

  PreferredSizeChanged();
}

BEGIN_METADATA(CameraRollView)
END_METADATA

}  // namespace ash
