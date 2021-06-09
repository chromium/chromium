// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include <algorithm>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
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
constexpr int kSecondaryActionIconSize = 16;

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
    HoldingSpaceViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kPadding, kChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetPreferredSize(gfx::Size(kPreferredWidth, kPreferredHeight));

  auto* image_checkmark_and_secondary_action_container =
      AddChildView(std::make_unique<views::View>());
  image_checkmark_and_secondary_action_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  // Image.
  image_ = image_checkmark_and_secondary_action_container->AddChildView(
      std::make_unique<RoundedImageView>(
          kHoldingSpaceChipIconSize / 2,
          RoundedImageView::Alignment::kLeading));
  image_->SetID(kHoldingSpaceItemImageId);

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();

  // Checkmark.
  AddCheckmark(/*parent=*/image_checkmark_and_secondary_action_container);

  // Secondary action.
  secondary_action_container_ =
      image_checkmark_and_secondary_action_container->AddChildView(
          std::make_unique<views::View>());
  secondary_action_container_->SetID(
      kHoldingSpaceItemSecondaryActionContainerId);
  secondary_action_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  secondary_action_container_->SetVisible(false);

  // Pause.
  secondary_action_pause_ = secondary_action_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &HoldingSpaceItemChipView::OnSecondaryActionPressed,
          base::Unretained(this))));
  secondary_action_pause_->SetID(kHoldingSpaceItemPauseButtonId);
  secondary_action_pause_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  secondary_action_pause_->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  secondary_action_pause_->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  secondary_action_pause_->SetVisible(false);

  // Resume.
  secondary_action_resume_ = secondary_action_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &HoldingSpaceItemChipView::OnSecondaryActionPressed,
          base::Unretained(this))));
  secondary_action_resume_->SetID(kHoldingSpaceItemResumeButtonId);
  secondary_action_resume_->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  secondary_action_resume_->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  secondary_action_resume_->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  secondary_action_resume_->SetVisible(false);

  auto* label_and_primary_action_container =
      AddChildView(std::make_unique<views::View>());
  label_and_primary_action_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  layout->SetFlexForView(label_and_primary_action_container, 1);

  // Label.
  // NOTE: A11y events for `label_` are handled by its parent.
  label_ = label_and_primary_action_container->AddChildView(
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

  bubble_utils::ApplyStyle(label_, bubble_utils::LabelStyle::kChip);

  // Primary action.
  views::View* primary_action_container =
      label_and_primary_action_container->AddChildView(
          std::make_unique<views::View>());

  layout = primary_action_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddPrimaryAction(/*parent=*/primary_action_container,
                   /*min_size=*/gfx::Size());
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
  if (this->item() == item) {
    label_->SetText(item->text());
    UpdateSecondaryAction();
  }
}

void HoldingSpaceItemChipView::OnPrimaryActionVisibilityChanged(bool visible) {
  // The `label_` must be repainted to update its mask for
  // `primary_action_container()`  visibility.
  label_->SchedulePaint();
}

void HoldingSpaceItemChipView::OnSelectionUiChanged() {
  HoldingSpaceItemView::OnSelectionUiChanged();
  UpdateLabel();
  UpdateSecondaryAction();
}

void HoldingSpaceItemChipView::OnMouseEvent(ui::MouseEvent* event) {
  HoldingSpaceItemView::OnMouseEvent(event);
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateSecondaryAction();
      break;
    default:
      break;
  }
}

void HoldingSpaceItemChipView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();
  UpdateImage();
  UpdateLabel();

  // Pause.
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  secondary_action_pause_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kPauseIcon, kSecondaryActionIconSize, icon_color));

  // Resume.
  secondary_action_resume_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kResumeIcon, kSecondaryActionIconSize, icon_color));
}

void HoldingSpaceItemChipView::OnPaintLabelMask(gfx::Canvas* canvas) {
  // If the `primary_action_container()` isn't visible, masking is unnecessary.
  if (!primary_action_container()->GetVisible())
    return;

  // If the `primary_action_container()` is visible, `label_` fades out its tail
  // to avoid overlap.
  gfx::Point gradient_start, gradient_end;
  if (base::i18n::IsRTL()) {
    gradient_end.set_x(primary_action_container()->width());
    gradient_start.set_x(gradient_end.x() + kLabelMaskGradientWidth);
  } else {
    gradient_end.set_x(label_->width() - primary_action_container()->width());
    gradient_start.set_x(gradient_end.x() - kLabelMaskGradientWidth);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kDstIn);
  flags.setShader(gfx::CreateGradientShader(
      gradient_start, gradient_end, SK_ColorBLACK, SK_ColorTRANSPARENT));

  canvas->DrawRect(label_->GetLocalBounds(), flags);
}

void HoldingSpaceItemChipView::OnSecondaryActionPressed() {
  DCHECK_NE(secondary_action_pause_->GetVisible(),
            secondary_action_resume_->GetVisible());

  // Pause.
  if (secondary_action_pause_->GetVisible()) {
    HoldingSpaceController::Get()->client()->PauseItems({item()});
    return;
  }

  // Resume.
  HoldingSpaceController::Get()->client()->ResumeItems({item()});
}

void HoldingSpaceItemChipView::UpdateImage() {
  image_->SetImage(item()->image().GetImageSkia(
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize),
      /*dark_background=*/AshColorProvider::Get()->IsDarkModeEnabled()));
  SchedulePaint();
}

void HoldingSpaceItemChipView::UpdateLabel() {
  const bool multiselect = delegate()->selection_ui() ==
                           HoldingSpaceViewDelegate::SelectionUi::kMultiSelect;

  label_->SetEnabledColor(
      selected() && multiselect
          ? GetMultiSelectTextColor()
          : AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorPrimary));
}

void HoldingSpaceItemChipView::UpdateSecondaryAction() {
  const bool has_secondary_action =
      !checkmark()->GetVisible() && item()->IsInProgress() && IsMouseHovered();

  if (!has_secondary_action) {
    image_->SetVisible(!checkmark()->GetVisible());
    secondary_action_container_->SetVisible(false);
    return;
  }

  // Pause/resume.
  const bool is_item_paused = item()->IsPaused();
  secondary_action_pause_->SetVisible(!is_item_paused);
  secondary_action_resume_->SetVisible(is_item_paused);

  image_->SetVisible(false);
  secondary_action_container_->SetVisible(true);
}

BEGIN_METADATA(HoldingSpaceItemChipView, HoldingSpaceItemView)
END_METADATA

}  // namespace ash
