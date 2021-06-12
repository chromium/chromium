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
#include "ash/system/holding_space/holding_space_view_builder.h"
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
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/view_factory.h"

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

// PaintCallbackLabel ----------------------------------------------------------

class PaintCallbackLabel : public views::Label {
 public:
  PaintCallbackLabel() = default;
  PaintCallbackLabel(const PaintCallbackLabel&) = delete;
  PaintCallbackLabel& operator=(const PaintCallbackLabel&) = delete;
  ~PaintCallbackLabel() override = default;

  using Callback = base::RepeatingCallback<void(gfx::Canvas* canvas)>;
  void SetCallback(Callback callback) { callback_ = std::move(callback); }

 private:
  // views::Label:
  void OnPaint(gfx::Canvas* canvas) override {
    views::Label::OnPaint(canvas);
    if (!callback_.is_null())
      callback_.Run(canvas);
  }

  Callback callback_;
};

BEGIN_VIEW_BUILDER(/*no export*/, PaintCallbackLabel, views::Label)
VIEW_BUILDER_PROPERTY(PaintCallbackLabel::Callback, Callback)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::PaintCallbackLabel)

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns a label builder with desired `style`, `text` and paint `callback`.
std::unique_ptr<HoldingSpaceViewBuilder<views::Label>> CreateLabelBuilder(
    bubble_utils::LabelStyle style,
    const std::u16string& text,
    PaintCallbackLabel::Callback callback) {
  auto label = views::Builder<PaintCallbackLabel>()
                   .SetBorder(views::CreateEmptyBorder(kLabelMargins))
                   .SetCallback(std::move(callback))
                   .SetElideBehavior(gfx::ELIDE_MIDDLE)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .SetPaintToLayer()
                   .SetText(text)
                   .Build();

  // NOTE: A11y events are handled by `HoldingSpaceItemChipView`.
  label->GetViewAccessibility().OverrideIsIgnored(true);
  label->layer()->SetFillsBoundsOpaquely(false);
  bubble_utils::ApplyStyle(label.get(), style);
  return std::make_unique<HoldingSpaceViewBuilder<views::Label>>(
      std::move(label));
}

// Returns a secondary action builder that invokes `callback` on press.
std::unique_ptr<HoldingSpaceViewBuilder<views::ImageButton>>
CreateSecondaryActionBuilder(views::ImageButton::PressedCallback callback) {
  using HorizontalAlignment = views::ImageButton::HorizontalAlignment;
  using VerticalAlignment = views::ImageButton::VerticalAlignment;
  return std::make_unique<HoldingSpaceViewBuilder<views::ImageButton>>(
      views::Builder<views::ImageButton>()
          .SetCallback(std::move(callback))
          .SetFocusBehavior(views::View::FocusBehavior::NEVER)
          .SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER)
          .SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE)
          .SetVisible(false));
}

// TODO(crbug.com/1202796): Create ash colors.
// Returns the theme color to use for text in multiselect.
SkColor GetMultiSelectTextColor() {
  return AshColorProvider::Get()->IsDarkModeEnabled() ? gfx::kGoogleBlue100
                                                      : gfx::kGoogleBlue800;
}

}  // namespace

// HoldingSpaceItemChipView ----------------------------------------------------

HoldingSpaceItemChipView::HoldingSpaceItemChipView(
    HoldingSpaceViewDelegate* delegate,
    const HoldingSpaceItem* item)
    : HoldingSpaceItemView(delegate, item) {
  using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
  using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;
  using Orientation = views::BoxLayout::Orientation;

  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetInteriorMargin(gfx::Insets(kPadding))
      .SetDefault(views::kMarginsKey, gfx::Insets(0, kChildSpacing));

  auto paint_label_mask_callback = base::BindRepeating(
      &HoldingSpaceItemChipView::OnPaintLabelMask, base::Unretained(this));

  auto secondary_action_callback =
      base::BindRepeating(&HoldingSpaceItemChipView::OnSecondaryActionPressed,
                          base::Unretained(this));

  HoldingSpaceViewBuilder<HoldingSpaceItemChipView>(this)
      .SetPreferredSize(gfx::Size(kPreferredWidth, kPreferredHeight))
      .SetLayoutManager(std::move(layout_manager))
      .AddChild(
          HoldingSpaceViewBuilder<views::View>(
              views::Builder<views::View>().SetUseDefaultFillLayout(true))
              .AddChild(views::Builder<RoundedImageView>()
                            .CopyAddressTo(&image_)
                            .SetID(kHoldingSpaceItemImageId)
                            .SetCornerRadius(kHoldingSpaceChipIconSize / 2))
              .AddChild(CreateCheckmark())
              .AddChild(
                  HoldingSpaceViewBuilder<views::View>(
                      views::Builder<views::View>()
                          .CopyAddressTo(&secondary_action_container_)
                          .SetID(kHoldingSpaceItemSecondaryActionContainerId)
                          .SetUseDefaultFillLayout(true)
                          .SetVisible(false))
                      .AddChild(CreateSecondaryActionBuilder(
                                    secondary_action_callback)
                                    ->CopyAddressTo(&secondary_action_pause_)
                                    .SetID(kHoldingSpaceItemPauseButtonId))
                      .AddChild(CreateSecondaryActionBuilder(
                                    secondary_action_callback)
                                    ->CopyAddressTo(&secondary_action_resume_)
                                    .SetID(kHoldingSpaceItemResumeButtonId))))
      .AddChild(
          HoldingSpaceViewBuilder<views::View>(
              views::Builder<views::View>()
                  .SetUseDefaultFillLayout(true)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)))
              .AddChild(CreateLabelBuilder(bubble_utils::LabelStyle::kChip,
                                           item->text(),
                                           paint_label_mask_callback)
                            ->CopyAddressTo(&label_))
              .AddChild(
                  HoldingSpaceViewBuilder<views::BoxLayoutView>(
                      views::Builder<views::BoxLayoutView>()
                          .SetOrientation(Orientation::kHorizontal)
                          .SetMainAxisAlignment(MainAxisAlignment::kEnd)
                          .SetCrossAxisAlignment(CrossAxisAlignment::kCenter))
                      .AddChild(CreatePrimaryAction(/*min_size=*/gfx::Size()))))
      .BuildChildren();

  // Subscribe to be notified of changes to `item_`'s image.
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  UpdateImage();
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
