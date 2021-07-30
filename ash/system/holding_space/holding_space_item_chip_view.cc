// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Appearance.
constexpr int kChildSpacing = 8;
constexpr int kLabelMaskGradientWidth = 16;
constexpr gfx::Insets kLabelMargins(0, 0, 0, /*right=*/2);
constexpr gfx::Insets kPadding(8, 8, 8, /*right=*/10);
constexpr int kPreferredHeight = 40;
constexpr int kPreferredWidth = 160;

// Helpers ---------------------------------------------------------------------

// TODO(crbug.com/1202796): Create ash colors.
SkColor GetMultiSelectTextColor() {
  return AshColorProvider::Get()->IsDarkModeEnabled() ? gfx::kGoogleBlue100
                                                      : gfx::kGoogleBlue800;
}

// PaintCallbackLabel ----------------------------------------------------------

class PaintCallbackLabel : public views::Label {
 public:
  using PaintCallback = base::RepeatingCallback<void(gfx::Canvas* canvas)>;

  explicit PaintCallbackLabel(PaintCallback callback) : callback_(callback) {}
  PaintCallbackLabel(const PaintCallbackLabel&) = delete;
  PaintCallbackLabel& operator=(const PaintCallbackLabel&) = delete;
  ~PaintCallbackLabel() override = default;

 private:
  // views::Label:
  void OnPaint(gfx::Canvas* canvas) override {
    views::Label::OnPaint(canvas);
    callback_.Run(canvas);
  }

  PaintCallback callback_;
};

}  // namespace

// HoldingSpaceItemChipView ----------------------------------------------------

HoldingSpaceItemChipView::HoldingSpaceItemChipView(
    HoldingSpaceItemViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kPadding, kChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(kPreferredWidth, kPreferredHeight));

  auto* image_and_checkmark_container =
      AddChildView(std::make_unique<views::View>());
  image_and_checkmark_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  // Image.
  image_ = image_and_checkmark_container->AddChildView(
      std::make_unique<RoundedImageView>(
          kHoldingSpaceChipIconSize / 2,
          RoundedImageView::Alignment::kLeading));
  image_->SetID(kHoldingSpaceItemImageId);

  // Subscribe to be notified of changes to `item`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();

  // Checkmark.
  AddCheckmark(/*parent=*/image_and_checkmark_container);

  auto* label_and_pin_button_container =
      AddChildView(std::make_unique<views::View>());
  label_and_pin_button_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  layout->SetFlexForView(label_and_pin_button_container, 1);

  // Label.
  // NOTE: A11y events for `label_` are handled by its parent.
  label_ = label_and_pin_button_container->AddChildView(
      std::make_unique<PaintCallbackLabel>(
          base::BindRepeating(&HoldingSpaceItemChipView::OnPaintLabelMask,
                              base::Unretained(this))));
  label_->GetViewAccessibility().OverrideIsIgnored(true);
  label_->SetBorder(views::CreateEmptyBorder(kLabelMargins));
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetText(item->text());
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);

  holding_space_util::ApplyStyle(label_, holding_space_util::LabelStyle::kChip);

  // Pin.
  views::View* pin_button_container =
      label_and_pin_button_container->AddChildView(
          std::make_unique<views::View>());

  auto* pin_layout =
      pin_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  pin_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  pin_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddPin(/*parent=*/pin_button_container);
}

HoldingSpaceItemChipView::~HoldingSpaceItemChipView() = default;

views::View* HoldingSpaceItemChipView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltips for this view are handled by `label_`, which will only show
  // tooltips if the underlying text has been elided due to insufficient space.
  return HitTestPoint(point) ? label_ : nullptr;
}

void HoldingSpaceItemChipView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item) {
  HoldingSpaceItemView::OnHoldingSpaceItemUpdated(item);
  if (this->item() == item)
    label_->SetText(item->text());
}

void HoldingSpaceItemChipView::OnPinVisibilityChanged(bool pin_visible) {
  // The `label_` must be repainted to update its mask for `pin()` visibility.
  label_->SchedulePaint();
}

void HoldingSpaceItemChipView::OnSelectionUiChanged() {
  HoldingSpaceItemView::OnSelectionUiChanged();

  const bool multiselect =
      delegate()->selection_ui() ==
      HoldingSpaceItemViewDelegate::SelectionUi::kMultiSelect;

  image_->SetVisible(!selected() || !multiselect);
  UpdateLabel();
}

void HoldingSpaceItemChipView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();
  UpdateImage();
  UpdateLabel();
}

void HoldingSpaceItemChipView::OnPaintLabelMask(gfx::Canvas* canvas) {
  // When the `pin()` is not visible no masking is necessary.
  if (!pin()->GetVisible())
    return;

  // When the `pin()` is visible, `label_` fades out its tail to avoid overlap.
  gfx::Point gradient_start, gradient_end;
  if (base::i18n::IsRTL()) {
    gradient_end.set_x(pin()->width());
    gradient_start.set_x(gradient_end.x() + kLabelMaskGradientWidth);
  } else {
    gradient_end.set_x(label_->width() - pin()->width());
    gradient_start.set_x(gradient_end.x() - kLabelMaskGradientWidth);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kDstIn);
  flags.setShader(gfx::CreateGradientShader(
      gradient_start, gradient_end, SK_ColorBLACK, SK_ColorTRANSPARENT));

  canvas->DrawRect(label_->GetLocalBounds(), flags);
}

void HoldingSpaceItemChipView::UpdateImage() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  image_->SetImage(item()->image().GetImageSkia(
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize),
      /*dark_background=*/AshColorProvider::Get()->IsDarkModeEnabled()));
  SchedulePaint();
}

void HoldingSpaceItemChipView::UpdateLabel() {
  const bool multiselect =
      delegate()->selection_ui() ==
      HoldingSpaceItemViewDelegate::SelectionUi::kMultiSelect;

  label_->SetEnabledColor(
      selected() && multiselect
          ? GetMultiSelectTextColor()
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary));
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
