// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

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
    gfx::Point gradient_start(
        gradient_end.x() - kHoldingSpaceChipLabelMaskGradientWidth,
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
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kHoldingSpaceChipPadding), kHoldingSpaceChipChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(kHoldingSpaceChipWidth, kHoldingSpaceChipHeight));

  image_ = AddChildView(std::make_unique<RoundedImageView>(
      kHoldingSpaceChipIconSize / 2, RoundedImageView::Alignment::kLeading));

  label_and_pin_button_container_ =
      AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(label_and_pin_button_container_, 1);

  label_and_pin_button_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  label_ = label_and_pin_button_container_->AddChildView(
      holding_space_util::CreateLabel(holding_space_util::LabelStyle::kChip));
  label_->SetElideBehavior(gfx::ELIDE_MIDDLE);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetText(item->text());

  label_mask_layer_owner_ = std::make_unique<LabelMaskLayerOwner>();

  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->layer()->SetMaskLayer(label_mask_layer_owner_->layer());

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();

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
  image_->SetImage(
      item()->image().image_skia(),
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize));
  SchedulePaint();
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
