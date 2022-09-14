// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy/privacy_indicators_tray_item_view.h"

#include <memory>
#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr auto kPrivacyIndicatorsViewPadding = gfx::Insets::VH(4, 8);
const int kPrivacyIndicatorsViewSpacing = 2;
const int kPrivacyIndicatorsIconSize = 16;
const int kPrivacyIndicatorsViewHeight = 24;
const int kPrivacyIndicatorsViewWidth = 50;

}  // namespace

PrivacyIndicatorsTrayItemView::PrivacyIndicatorsTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  SetVisible(false);

  auto container_view = std::make_unique<views::View>();
  layout_manager_ =
      container_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                                  views::BoxLayout::Orientation::kVertical),
          kPrivacyIndicatorsViewPadding, kPrivacyIndicatorsViewSpacing));
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Set up a solid color layer to paint the background color, then add a layer
  // to each child so that they are visible and can perform layer animation.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{kPrivacyIndicatorsViewHeight / 2});

  auto camera_icon = std::make_unique<views::ImageView>();
  camera_icon->SetPaintToLayer();
  camera_icon->layer()->SetFillsBoundsOpaquely(false);
  camera_icon_ = container_view->AddChildView(std::move(camera_icon));

  auto microphone_icon = std::make_unique<views::ImageView>();
  microphone_icon->SetPaintToLayer();
  microphone_icon->layer()->SetFillsBoundsOpaquely(false);
  microphone_icon_ = container_view->AddChildView(std::move(microphone_icon));

  AddChildView(std::move(container_view));

  UpdateIcons();
}

PrivacyIndicatorsTrayItemView::~PrivacyIndicatorsTrayItemView() = default;

void PrivacyIndicatorsTrayItemView::Update(bool camera_is_used,
                                           bool microphone_is_used) {
  if (camera_is_used_ == camera_is_used &&
      microphone_is_used_ == microphone_is_used) {
    return;
  }
  camera_is_used_ = camera_is_used;
  microphone_is_used_ = microphone_is_used;

  SetVisible(camera_is_used_ || microphone_is_used_);
  if (!GetVisible())
    return;

  camera_icon_->SetVisible(camera_is_used);
  microphone_icon_->SetVisible(microphone_is_used);

  TooltipTextChanged();
}

void PrivacyIndicatorsTrayItemView::UpdateAlignmentForShelf(Shelf* shelf) {
  layout_manager_->SetOrientation(
      shelf->PrimaryAxisValue(views::BoxLayout::Orientation::kHorizontal,
                              views::BoxLayout::Orientation::kVertical));
  UpdateBoundsInset();
}

void PrivacyIndicatorsTrayItemView::HandleLocaleChange() {
  TooltipTextChanged();
}

gfx::Size PrivacyIndicatorsTrayItemView::CalculatePreferredSize() const {
  // If the view is laid out vertically in vertical shelf, the view is rotated
  // 90 degree.
  if (layout_manager_->GetOrientation() ==
      views::BoxLayout::Orientation::kVertical) {
    return gfx::Size(kPrivacyIndicatorsViewHeight, kPrivacyIndicatorsViewWidth);
  }
  return gfx::Size(kPrivacyIndicatorsViewWidth, kPrivacyIndicatorsViewHeight);
}

void PrivacyIndicatorsTrayItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIcons();

  layer()->SetColor(
      GetColorProvider()->GetColor(ui::kColorAshPrivacyIndicatorsBackground));
}

void PrivacyIndicatorsTrayItemView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateBoundsInset();
}

std::u16string PrivacyIndicatorsTrayItemView::GetTooltipText(
    const gfx::Point& p) const {
  if (camera_is_used_ && microphone_is_used_) {
    return l10n_util::GetStringUTF16(
        IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA_AND_MIC);
  }

  if (camera_is_used_)
    return l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_CAMERA);

  if (microphone_is_used_)
    return l10n_util::GetStringUTF16(IDS_PRIVACY_NOTIFICATION_TITLE_MIC);

  return std::u16string();
}

views::View* PrivacyIndicatorsTrayItemView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

const char* PrivacyIndicatorsTrayItemView::GetClassName() const {
  return "PrivacyIndicatorsTrayItemView";
}

void PrivacyIndicatorsTrayItemView::UpdateIcons() {
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  camera_icon_->SetImage(gfx::CreateVectorIcon(
      kPrivacyIndicatorsCameraIcon, kPrivacyIndicatorsIconSize, icon_color));
  microphone_icon_->SetImage(
      gfx::CreateVectorIcon(kPrivacyIndicatorsMicrophoneIcon,
                            kPrivacyIndicatorsIconSize, icon_color));
}

void PrivacyIndicatorsTrayItemView::UpdateBoundsInset() {
  gfx::Rect bounds = GetLocalBounds();
  auto* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());

  // We set the bounds inset based on the shorter side of the view (the shorter
  // size changes based on shelf alignment).
  int shorter_side_inset = shelf->PrimaryAxisValue(height(), width()) -
                           shelf->PrimaryAxisValue(GetPreferredSize().height(),
                                                   GetPreferredSize().width());
  bounds.Inset(
      shelf->PrimaryAxisValue(gfx::Insets::VH(shorter_side_inset / 2, 0),
                              gfx::Insets::VH(0, shorter_side_inset / 2)));
  layer()->SetClipRect(bounds);
}

}  // namespace ash
