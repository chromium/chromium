// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_screen_capture_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {
namespace {

// Appearance.
constexpr int kBorderThickness = 1;
constexpr gfx::Insets kCheckmarkAndPrimaryActionContainerPadding(4);
constexpr gfx::Size kOverlayIconSize(32, 32);
constexpr gfx::Size kPrimaryActionSize(24, 24);

// Helpers ---------------------------------------------------------------------

std::optional<const gfx::VectorIcon*> GetOverlayIcon(
    const HoldingSpaceItem* item) {
  DCHECK(HoldingSpaceItem::IsScreenCaptureType(item->type()));
  switch (item->type()) {
    case HoldingSpaceItem::Type::kScreenRecording:
      return &vector_icons::kPlayArrowIcon;
    case HoldingSpaceItem::Type::kScreenRecordingGif:
      return &kGifIcon;
    case HoldingSpaceItem::Type::kArcDownload:
    case HoldingSpaceItem::Type::kDiagnosticsLog:
    case HoldingSpaceItem::Type::kDriveSuggestion:
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kLacrosDownload:
    case HoldingSpaceItem::Type::kLocalSuggestion:
    case HoldingSpaceItem::Type::kNearbyShare:
    case HoldingSpaceItem::Type::kPhoneHubCameraRoll:
    case HoldingSpaceItem::Type::kPhotoshopWeb:
    case HoldingSpaceItem::Type::kPinnedFile:
    case HoldingSpaceItem::Type::kPrintedPdf:
    case HoldingSpaceItem::Type::kScan:
      NOTREACHED();
    case HoldingSpaceItem::Type::kScreenshot:
      return std::nullopt;
  }
}

}  // namespace

// HoldingSpaceItemScreenCaptureView -------------------------------------------

HoldingSpaceItemScreenCaptureView::HoldingSpaceItemScreenCaptureView(
    HoldingSpaceViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
  using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;

  views::Builder<HoldingSpaceItemScreenCaptureView> builder(this);
  builder.SetPreferredSize(kHoldingSpaceScreenCaptureSize)
      .SetLayoutManager(std::make_unique<views::FillLayout>())
      .AddChild(views::Builder<RoundedImageView>()
                    .CopyAddressTo(&image_)
                    .SetID(kHoldingSpaceItemImageId)
                    .SetCornerRadius(kHoldingSpaceCornerRadius));

  if (std::optional<const gfx::VectorIcon*> overlay_icon =
          GetOverlayIcon(item)) {
    builder.AddChild(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetMainAxisAlignment(MainAxisAlignment::kCenter)
            .SetCrossAxisAlignment(CrossAxisAlignment::kCenter)
            .SetFocusBehavior(views::View::FocusBehavior::NEVER)
            .AddChild(
                views::Builder<views::ImageView>()
                    .SetID(kHoldingSpaceScreenCaptureOverlayIconId)
                    .SetPreferredSize(kOverlayIconSize)
                    .SetImageSize(
                        gfx::Size(kHoldingSpaceIconSize, kHoldingSpaceIconSize))
                    .SetImage(ui::ImageModel::FromVectorIcon(
                        *overlay_icon.value(), kColorAshButtonIconColor,
                        kHoldingSpaceIconSize))
                    .SetBackground(holding_space_util::CreateCircleBackground(
                        kColorAshShieldAndBase80))));
  }

  std::move(builder)
      .AddChild(
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetInteriorMargin(kCheckmarkAndPrimaryActionContainerPadding)
              .AddChild(CreateCheckmarkBuilder())
              .AddChild(views::Builder<views::View>().SetProperty(
                  views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kScaleToZero,
                      views::MaximumFlexSizeRule::kUnbounded)))
              .AddChild(
                  CreatePrimaryActionBuilder(
                      /*apply_accent_colors=*/true,
                      /*min_size=*/kPrimaryActionSize)
                      .SetBackground(holding_space_util::CreateCircleBackground(
                          kColorAshShieldAndBase80))))
      .AddChild(views::Builder<views::View>()
                    .SetCanProcessEventsWithinSubtree(false)
                    .SetBorder(views::CreateThemedRoundedRectBorder(
                        kBorderThickness, kHoldingSpaceCornerRadius,
                        kColorAshSeparatorColor)))
      .BuildChildren();

  // Subscribe to be notified of changes to `item`'s image.
  image_skia_changed_subscription_ = item->image().AddImageSkiaChangedCallback(
      base::BindRepeating(&HoldingSpaceItemScreenCaptureView::UpdateImage,
                          base::Unretained(this)));

  UpdateImage();
}

HoldingSpaceItemScreenCaptureView::~HoldingSpaceItemScreenCaptureView() =
    default;

views::View* HoldingSpaceItemScreenCaptureView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string HoldingSpaceItemScreenCaptureView::GetTooltipText(
    const gfx::Point& point) const {
  return item() ? item()->GetText() : std::u16string();
}

void HoldingSpaceItemScreenCaptureView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    const HoldingSpaceItemUpdatedFields& updated_fields) {
  HoldingSpaceItemView::OnHoldingSpaceItemUpdated(item, updated_fields);
  if (this->item() == item)
    TooltipTextChanged();
}

void HoldingSpaceItemScreenCaptureView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();

  UpdateImage();
}

void HoldingSpaceItemScreenCaptureView::UpdateImage() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  image_->SetImage(item()->image().GetImageSkia(
      kHoldingSpaceScreenCaptureSize,
      /*dark_background=*/DarkLightModeControllerImpl::Get()
          ->IsDarkModeEnabled()));
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemScreenCaptureView)
END_METADATA

}  // namespace ash
