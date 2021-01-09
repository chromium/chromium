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
#include "ash/system/holding_space/holding_space_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/painter.h"

namespace ash {

// Appearance.
constexpr int kChildSpacing = 8;
constexpr int kLabelMaskGradientWidth = 16;
constexpr gfx::Insets kLabelMargins(0, 0, 0, /*right=*/2);
constexpr gfx::Insets kPadding(8, 8, 8, /*right=*/10);
constexpr int kPreferredHeight = 40;
constexpr int kPreferredWidth = 160;

// CirclePainter ---------------------------------------------------------------

class CirclePainter : public views::Painter {
 public:
  CirclePainter(SkColor color, const gfx::InsetsF& insets)
      : color_(color), insets_(insets) {}
  CirclePainter(const CirclePainter&) = delete;
  CirclePainter& operator=(const CirclePainter&) = delete;
  ~CirclePainter() override = default;

 private:
  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::RectF bounds{gfx::SizeF(size)};
    bounds.Inset(insets_);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);

    canvas->DrawCircle(
        bounds.CenterPoint(),
        std::min(bounds.size().width(), bounds.size().height()) / 2.f, flags);
  }

  const SkColor color_;
  const gfx::InsetsF insets_;
};

// HoldingSpaceItemChipView::LabelMaskOwner ------------------------------------

class HoldingSpaceItemChipView::LabelMaskLayerOwner : public ui::LayerDelegate {
 public:
  LabelMaskLayerOwner() : layer_(ui::LAYER_TEXTURED) {
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  LabelMaskLayerOwner(const LabelMaskLayerOwner&) = delete;
  LabelMaskLayerOwner& operator=(const LabelMaskLayerOwner&) = delete;

  ~LabelMaskLayerOwner() override { layer_.set_delegate(nullptr); }

  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    const gfx::Size size = layer()->size();

    views::PaintInfo paint_info =
        views::PaintInfo::CreateRootPaintInfo(context, size);
    const auto& paint_recording_size = paint_info.paint_recording_size();

    // Pass the scale factor when constructing `PaintRecorder` so the mask layer
    // size is not incorrectly rounded (see https://crbug.com/921274).
    ui::PaintRecorder recorder(
        context, paint_info.paint_recording_size(),
        static_cast<float>(paint_recording_size.width()) / size.width(),
        static_cast<float>(paint_recording_size.height()) / size.height(),
        /*cache*/ nullptr);

    cc::PaintFlags flags;
    flags.setAntiAlias(false);

    gfx::Point gradient_end(size.width() - kHoldingSpaceIconSize, 0);
    gfx::Point gradient_start(gradient_end.x() - kLabelMaskGradientWidth,
                              gradient_end.y());
    flags.setShader(gfx::CreateGradientShader(
        gradient_start, gradient_end, SK_ColorBLACK, SK_ColorTRANSPARENT));

    recorder.canvas()->DrawRect(gfx::Rect(size), flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  ui::Layer layer_;
};

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

  image_ = AddChildView(std::make_unique<RoundedImageView>(
      kHoldingSpaceChipIconSize / 2, RoundedImageView::Alignment::kLeading));

  // Shrink circular background by a single pixel to prevent painting outside of
  // the image which may otherwise occur due to pixel rounding. Failure to do so
  // could result in white paint artifacts.
  image_->SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<CirclePainter>(SK_ColorWHITE, gfx::InsetsF(0.5f))));

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();

  label_and_pin_button_container_ =
      AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(label_and_pin_button_container_, 1);

  label_and_pin_button_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  label_ = label_and_pin_button_container_->AddChildView(
      holding_space_util::CreateLabel(holding_space_util::LabelStyle::kChip));
  label_->SetBorder(views::CreateEmptyBorder(kLabelMargins));
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetText(item->text());

  label_mask_layer_owner_ = std::make_unique<LabelMaskLayerOwner>();

  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetMaskLayer(label_mask_layer_owner_->layer());

  views::View* pin_button_container =
      label_and_pin_button_container_->AddChildView(
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

void HoldingSpaceItemChipView::OnPinVisiblityChanged(bool pin_visible) {
  if (label_mask_layer_owner_->layer()->bounds() !=
      label_and_pin_button_container_->bounds()) {
    // Mask layer has the same size as the label container so that the gradient
    // ends at the end of the container.
    label_mask_layer_owner_->layer()->SetBounds(
        label_and_pin_button_container_->bounds());
  }
  label_mask_layer_owner_->layer()->SetVisible(pin_visible);
}

void HoldingSpaceItemChipView::UpdateImage() {
  image_->SetImage(item()->image().GetImageSkia(
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize)));
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
