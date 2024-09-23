// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_chip_view.h"

#include <algorithm>
#include <optional>
#include <variant>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_colors.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_progress_indicator_util.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/system/progress_indicator/progress_indicator_animation_registry.h"
#include "ash/system/progress_indicator/progress_ring_animation.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
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

namespace ash {
namespace {

// Appearance.
constexpr int kChildSpacing = 8;
constexpr int kLabelMaskGradientWidth = 16;
constexpr auto kLabelMargins = gfx::Insets::TLBR(4, 0, 4, 2);
constexpr auto kPadding = gfx::Insets::TLBR(0, 8, 0, 10);
constexpr int kPreferredHeight = 40;
constexpr int kPreferredWidth = 160;
constexpr int kProgressIndicatorSize = 26;
constexpr int kSecondaryActionIconSize = 16;

// Animation.
constexpr base::TimeDelta kInProgressImageScaleDuration =
    base::Milliseconds(150);
constexpr float kInProgressImageScaleFactor = 0.7f;

// Helpers ---------------------------------------------------------------------

void ToCenteredSize(gfx::Rect* rect, const gfx::Size& size) {
  rect->Outset(gfx::Outsets::VH(size.height(), size.width()));
  rect->ClampToCenteredSize(size);
}

// ObservableRoundedImageView --------------------------------------------------

class ObservableRoundedImageView : public RoundedImageView {
  METADATA_HEADER(ObservableRoundedImageView, RoundedImageView)

 public:
  ObservableRoundedImageView() = default;
  ObservableRoundedImageView(const ObservableRoundedImageView&) = delete;
  ObservableRoundedImageView& operator=(const ObservableRoundedImageView&) =
      delete;
  ~ObservableRoundedImageView() override = default;

  using BoundsChangedCallback = base::RepeatingCallback<void()>;
  void SetBoundsChangedCallback(BoundsChangedCallback bounds_changed_callback) {
    bounds_changed_callback_ = std::move(bounds_changed_callback);
  }

 private:
  // RoundedImageView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    RoundedImageView::OnBoundsChanged(previous_bounds);
    if (!bounds_changed_callback_.is_null())
      bounds_changed_callback_.Run();
  }

  BoundsChangedCallback bounds_changed_callback_;
};

BEGIN_METADATA(ObservableRoundedImageView)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, ObservableRoundedImageView, RoundedImageView)
VIEW_BUILDER_PROPERTY(ObservableRoundedImageView::BoundsChangedCallback,
                      BoundsChangedCallback)
END_VIEW_BUILDER

// PaintCallbackLabel ----------------------------------------------------------

class PaintCallbackLabel : public views::Label {
  METADATA_HEADER(PaintCallbackLabel, views::Label)

 public:
  PaintCallbackLabel() = default;
  PaintCallbackLabel(const PaintCallbackLabel&) = delete;
  PaintCallbackLabel& operator=(const PaintCallbackLabel&) = delete;
  ~PaintCallbackLabel() override = default;

  using Callback = base::RepeatingCallback<void(views::Label*, gfx::Canvas*)>;
  void SetCallback(Callback callback) { callback_ = std::move(callback); }

  void SetPaintToLayer(bool fills_bounds_opaquely) {
    views::Label::SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(fills_bounds_opaquely);
  }

  void SetStyle(TypographyToken style) {
    bubble_utils::ApplyStyle(this, style);
  }

  void SetViewAccessibilityIsIgnored(bool is_ignored) {
    GetViewAccessibility().SetIsIgnored(is_ignored);
  }

 private:
  // views::Label:
  void OnPaint(gfx::Canvas* canvas) override {
    views::Label::OnPaint(canvas);
    if (!callback_.is_null())
      callback_.Run(this, canvas);
  }

  Callback callback_;
};

BEGIN_METADATA(PaintCallbackLabel)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, PaintCallbackLabel, views::Label)
VIEW_BUILDER_PROPERTY(PaintCallbackLabel::Callback, Callback)
VIEW_BUILDER_PROPERTY(TypographyToken, Style)
VIEW_BUILDER_PROPERTY(bool, PaintToLayer)
VIEW_BUILDER_PROPERTY(bool, ViewAccessibilityIsIgnored)
END_VIEW_BUILDER

// ProgressIndicatorView -------------------------------------------------------

class ProgressIndicatorView : public views::View {
  METADATA_HEADER(ProgressIndicatorView, views::View)

 public:
  ProgressIndicatorView() = default;
  ProgressIndicatorView(const ProgressIndicatorView&) = delete;
  ProgressIndicatorView& operator=(const ProgressIndicatorView&) = delete;
  ~ProgressIndicatorView() override = default;

  // Copies the address of `progress_indicator_` to the specified `ptr`.
  // NOTE: This method should only be invoked after `SetHoldingSpaceItem()`.
  void CopyProgressIndicatorAddressTo(raw_ptr<ProgressIndicator>* ptr) {
    DCHECK(progress_indicator_);
    *ptr = progress_indicator_.get();
  }

  // Sets the underlying `item` for which to indicate progress.
  // NOTE: This method should be invoked only once.
  void SetHoldingSpaceItem(const HoldingSpaceItem* item) {
    DCHECK(!progress_indicator_);
    progress_indicator_ =
        holding_space_util::CreateProgressIndicatorForItem(item);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->Add(progress_indicator_->CreateLayer(base::BindRepeating(
        [](const ProgressIndicatorView* self, ui::ColorId color_id) {
          return self->GetColorProvider()->GetColor(color_id);
        },
        base::Unretained(this))));
  }

 private:
  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    if (progress_indicator_) {
      gfx::Rect bounds(GetLocalBounds());
      ToCenteredSize(&bounds,
                     gfx::Size(kProgressIndicatorSize, kProgressIndicatorSize));
      progress_indicator_->layer()->SetBounds(bounds);
    }
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (progress_indicator_)
      progress_indicator_->InvalidateLayer();
  }

  std::unique_ptr<ProgressIndicator> progress_indicator_;
};

BEGIN_METADATA(ProgressIndicatorView)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, ProgressIndicatorView, views::View)
VIEW_BUILDER_METHOD(CopyProgressIndicatorAddressTo, raw_ptr<ProgressIndicator>*)
VIEW_BUILDER_PROPERTY(const HoldingSpaceItem*, HoldingSpaceItem)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::ObservableRoundedImageView)
DEFINE_VIEW_BUILDER(/*no export*/, ash::PaintCallbackLabel)
DEFINE_VIEW_BUILDER(/*no export*/, ash::ProgressIndicatorView)

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns a label builder.
// NOTE: A11y events are handled by `HoldingSpaceItemChipView`.
views::Builder<PaintCallbackLabel> CreateLabelBuilder() {
  auto label = views::Builder<PaintCallbackLabel>();
  label.SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetPaintToLayer(/*fills_bounds_opaquely=*/false)
      .SetViewAccessibilityIsIgnored(true);
  return label;
}

// Returns a secondary action builder.
views::Builder<views::ImageButton> CreateSecondaryActionBuilder() {
  using HorizontalAlignment = views::ImageButton::HorizontalAlignment;
  using VerticalAlignment = views::ImageButton::VerticalAlignment;
  auto secondary_action = views::Builder<views::ImageButton>();
  secondary_action.SetFocusBehavior(views::View::FocusBehavior::NEVER)
      .SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER)
      .SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE);
  return secondary_action;
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
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, kChildSpacing));

  auto paint_label_mask_callback = base::BindRepeating(
      &HoldingSpaceItemChipView::OnPaintLabelMask, base::Unretained(this));

  auto secondary_action_callback =
      base::BindRepeating(&HoldingSpaceItemChipView::OnSecondaryActionPressed,
                          base::Unretained(this));

  views::Builder<HoldingSpaceItemChipView>(this)
      .SetPreferredSize(gfx::Size(kPreferredWidth, kPreferredHeight))
      .SetLayoutManager(std::move(layout_manager))
      .AddChild(
          views::Builder<ProgressIndicatorView>()
              .SetHoldingSpaceItem(item)
              .CopyProgressIndicatorAddressTo(&progress_indicator_)
              .SetUseDefaultFillLayout(true)
              .AddChild(views::Builder<ObservableRoundedImageView>()
                            .SetCornerRadius(kHoldingSpaceChipIconSize / 2)
                            .SetBoundsChangedCallback(base::BindRepeating(
                                &HoldingSpaceItemChipView::UpdateImageTransform,
                                base::Unretained(this)))
                            .CopyAddressTo(&image_)
                            .SetID(kHoldingSpaceItemImageId))
              .AddChild(CreateCheckmarkBuilder())
              .AddChild(
                  views::Builder<views::View>()
                      .CopyAddressTo(&secondary_action_container_)
                      .SetID(kHoldingSpaceItemSecondaryActionContainerId)
                      .SetUseDefaultFillLayout(true)
                      .SetVisible(false)
                      .AddChild(
                          CreateSecondaryActionBuilder()
                              .CopyAddressTo(&secondary_action_pause_)
                              .SetID(kHoldingSpaceItemPauseButtonId)
                              .SetCallback(secondary_action_callback)
                              .SetVisible(false)
                              .SetImageModel(
                                  views::Button::STATE_NORMAL,
                                  ui::ImageModel::FromVectorIcon(
                                      kPauseIcon, kColorAshButtonIconColor,
                                      kSecondaryActionIconSize)))
                      .AddChild(
                          CreateSecondaryActionBuilder()
                              .CopyAddressTo(&secondary_action_resume_)
                              .SetID(kHoldingSpaceItemResumeButtonId)
                              .SetCallback(secondary_action_callback)
                              .SetFlipCanvasOnPaintForRTLUI(false)
                              .SetVisible(false)
                              .SetImageModel(
                                  views::Button::STATE_NORMAL,
                                  ui::ImageModel::FromVectorIcon(
                                      kResumeIcon, kColorAshButtonIconColor,
                                      kSecondaryActionIconSize)))))
      .AddChild(
          views::Builder<views::View>()
              .SetUseDefaultFillLayout(true)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
              .AddChild(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(Orientation::kVertical)
                      .SetMainAxisAlignment(MainAxisAlignment::kCenter)
                      .SetCrossAxisAlignment(CrossAxisAlignment::kStretch)
                      .SetInsideBorderInsets(kLabelMargins)
                      .AddChild(CreateLabelBuilder()
                                    .CopyAddressTo(&primary_label_)
                                    .SetID(kHoldingSpaceItemPrimaryChipLabelId)
                                    .SetStyle(TypographyToken::kCrosBody2)
                                    .SetElideBehavior(gfx::ELIDE_MIDDLE)
                                    .SetCallback(paint_label_mask_callback))
                      .AddChild(
                          CreateLabelBuilder()
                              .CopyAddressTo(&secondary_label_)
                              .SetID(kHoldingSpaceItemSecondaryChipLabelId)
                              .SetStyle(TypographyToken::kCrosLabel1)
                              .SetElideBehavior(gfx::FADE_TAIL)
                              .SetCallback(paint_label_mask_callback))
                      .AfterBuild(base::BindOnce(
                          [](HoldingSpaceItemChipView* self,
                             views::BoxLayoutView* box_layout_view) {
                            // Synchronize line heights between primary and
                            // secondary labels so that text will be vertically
                            // centered when both are shown despite differences
                            // in font sizes.
                            self->secondary_label_->SetLineHeight(
                                self->primary_label_->GetLineHeight());
                          },
                          base::Unretained(this))))
              .AddChild(views::Builder<views::BoxLayoutView>()
                            .SetOrientation(Orientation::kHorizontal)
                            .SetMainAxisAlignment(MainAxisAlignment::kEnd)
                            .SetCrossAxisAlignment(CrossAxisAlignment::kCenter)
                            .AddChild(CreatePrimaryActionBuilder())))
      .BuildChildren();

  // Subscribe to be notified of changes to `item`'s image.
  image_skia_changed_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceItemChipView::UpdateImage, base::Unretained(this)));

  // Subscribe to be notified of changes to `item`'s progress ring animation.
  progress_ring_animation_changed_subscription_ =
      HoldingSpaceAnimationRegistry::GetInstance()
          ->AddProgressRingAnimationChangedCallbackForKey(
              ProgressIndicatorAnimationRegistry::AsAnimationKey(item),
              base::IgnoreArgs<ProgressRingAnimation*>(base::BindRepeating(
                  &HoldingSpaceItemChipView::UpdateImageTransform,
                  base::Unretained(this))));

  UpdateImage();
  UpdateImageAndProgressIndicatorVisibility();
  UpdateLabels();
}

HoldingSpaceItemChipView::~HoldingSpaceItemChipView() = default;

views::View* HoldingSpaceItemChipView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  // Tooltip events should be handled top level, not by descendents.
  return HitTestPoint(point) ? this : nullptr;
}

std::u16string HoldingSpaceItemChipView::GetTooltipText(
    const gfx::Point& point) const {
  std::u16string primary_tooltip = primary_label_->GetTooltipText(point);
  std::u16string secondary_tooltip = secondary_label_->GetTooltipText(point);

  // If there is neither a primary nor a secondary tooltip which should be
  // shown, then there is no tooltip to be shown at all.
  if (primary_tooltip.empty() && secondary_tooltip.empty()) {
    return std::u16string();
  }

  // If there is no primary tooltip, fallback to using the primary text. This
  // would occur if the `primary_label_` is not elided in same way.
  if (primary_tooltip.empty())
    primary_tooltip = primary_label_->GetText();

  // If there is no secondary tooltip, fallback to using the secondary text.
  // This would occur if the `secondary_label_` is not elided in some way.
  if (secondary_tooltip.empty())
    secondary_tooltip = secondary_label_->GetText();

  // If there still is no secondary tooltip, only the primary tooltip should be
  // shown. This would occur if there is no visible `secondary_label_`.
  if (secondary_tooltip.empty())
    return primary_tooltip;

  // Otherwise, concatenate and return the primary and secondary tooltips. This
  // will look something of the form: "filename.txt, Paused, 10/100 MB".
  return l10n_util::GetStringFUTF16(
      IDS_ASH_HOLDING_SPACE_ITEM_A11Y_NAME_AND_TOOLTIP, primary_tooltip,
      secondary_tooltip);
}

void HoldingSpaceItemChipView::OnHoldingSpaceItemUpdated(
    const HoldingSpaceItem* item,
    const HoldingSpaceItemUpdatedFields& updated_fields) {
  HoldingSpaceItemView::OnHoldingSpaceItemUpdated(item, updated_fields);
  if (this->item() == item) {
    UpdateImage();
    UpdateLabels();
    UpdateSecondaryAction();
  }
}

void HoldingSpaceItemChipView::OnPrimaryActionVisibilityChanged(bool visible) {
  // Labels must be repainted to update their masks for
  // `primary_action_container()`  visibility.
  primary_label_->SchedulePaint();
  secondary_label_->SchedulePaint();
}

void HoldingSpaceItemChipView::OnSelectionUiChanged() {
  HoldingSpaceItemView::OnSelectionUiChanged();
  UpdateLabels();
  UpdateSecondaryAction();
}

void HoldingSpaceItemChipView::OnMouseEvent(ui::MouseEvent* event) {
  HoldingSpaceItemView::OnMouseEvent(event);
  switch (event->type()) {
    case ui::EventType::kMouseEntered:
    case ui::EventType::kMouseExited:
      UpdateSecondaryAction();
      break;
    default:
      break;
  }
}

void HoldingSpaceItemChipView::OnThemeChanged() {
  HoldingSpaceItemView::OnThemeChanged();

  UpdateImage();
  UpdateLabels();
}

void HoldingSpaceItemChipView::OnPaintLabelMask(views::Label* label,
                                                gfx::Canvas* canvas) {
  // If the `primary_action_container()` isn't visible, masking is unnecessary.
  if (!primary_action_container()->GetVisible())
    return;

  // If the `primary_action_container()` is visible, `label` fades out its tail
  // to avoid overlap.
  gfx::Point gradient_start, gradient_end;
  if (base::i18n::IsRTL()) {
    gradient_end.set_x(primary_action_container()->width());
    gradient_start.set_x(gradient_end.x() + kLabelMaskGradientWidth);
  } else {
    gradient_end.set_x(label->width() - primary_action_container()->width());
    gradient_start.set_x(gradient_end.x() - kLabelMaskGradientWidth);
  }

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setBlendMode(SkBlendMode::kDstIn);
  flags.setShader(gfx::CreateGradientShader(
      gradient_start, gradient_end, SK_ColorBLACK, SK_ColorTRANSPARENT));

  canvas->DrawRect(label->GetLocalBounds(), flags);
}

void HoldingSpaceItemChipView::OnSecondaryActionPressed() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  DCHECK_NE(secondary_action_pause_->GetVisible(),
            secondary_action_resume_->GetVisible());

  if (delegate())
    delegate()->OnHoldingSpaceItemViewSecondaryActionPressed(this);

  // Pause/Resume.
  const HoldingSpaceCommandId command_id =
      secondary_action_pause_->GetVisible()
          ? HoldingSpaceCommandId::kPauseItem
          : HoldingSpaceCommandId::kResumeItem;
  const bool success = holding_space_util::ExecuteInProgressCommand(
      item(), command_id,
      holding_space_metrics::EventSource::kHoldingSpaceItem);
  CHECK(success);
}

void HoldingSpaceItemChipView::UpdateImage() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  // Image.
  image_->SetImage(item()->image().GetImageSkia(
      gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize),
      /*dark_background=*/DarkLightModeControllerImpl::Get()
          ->IsDarkModeEnabled()));
  SchedulePaint();

  // Transform.
  UpdateImageTransform();
}

void HoldingSpaceItemChipView::UpdateImageAndProgressIndicatorVisibility() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  const bool is_secondary_action_visible =
      secondary_action_container_->GetVisible();

  // The inner icon of the `progress_indicator_` may be visible iff there is
  // no visible secondary action or multiselect UI.
  const bool is_progress_indicator_inner_icon_visible =
      !is_secondary_action_visible && !checkmark()->GetVisible();

  // Similarly, the `image_` may be visible iff there is no visible secondary
  // action or multiselect UI but additionally the `image_` may only be visible
  // when `progress` is hidden or complete.
  bool is_image_visible = is_progress_indicator_inner_icon_visible;
  const HoldingSpaceProgress& progress = item()->progress();
  is_image_visible &= progress.IsHidden() || progress.IsComplete();

  image_->SetVisible(is_image_visible);
  progress_indicator_->SetInnerIconVisible(
      is_progress_indicator_inner_icon_visible);
}

void HoldingSpaceItemChipView::UpdateImageTransform() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  // Wait until `image_` has been laid out before updating transform. Once
  // bounds have been set, `UpdateImageTransform()` will be invoked again.
  if (image_->bounds().IsEmpty())
    return;

  const bool is_item_visibly_in_progress =
      !item()->progress().IsHidden() && !item()->progress().IsComplete();

  const ProgressRingAnimation* progress_ring_animation =
      HoldingSpaceAnimationRegistry::GetInstance()
          ->GetProgressRingAnimationForKey(
              ProgressIndicatorAnimationRegistry::AsAnimationKey(item()));

  gfx::Transform transform;
  if (is_item_visibly_in_progress || progress_ring_animation) {
    transform = gfx::GetScaleTransform(image_->bounds().CenterPoint(),
                                       kInProgressImageScaleFactor);
  }

  if (!image_->layer()) {
    image_->SetPaintToLayer();
    image_->layer()->SetFillsBoundsOpaquely(false);
  }

  if (image_->layer()->GetTargetTransform() == transform)
    return;

  // Non-identity transforms take effect immediately so as not to cause overlap
  // between the `image_` and progress ring.
  if (!transform.IsIdentity()) {
    image_->layer()->SetTransform(transform);
    return;
  }

  // Identify transforms are animated.
  ui::ScopedLayerAnimationSettings settings(image_->layer()->GetAnimator());
  settings.SetTransitionDuration(kInProgressImageScaleDuration);
  settings.SetTweenType(gfx::Tween::Type::LINEAR_OUT_SLOW_IN);
  image_->layer()->SetTransform(transform);
}

void HoldingSpaceItemChipView::UpdateLabels() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  const bool multiselect =
      delegate() && delegate()->selection_ui() ==
                        HoldingSpaceViewDelegate::SelectionUi::kMultiSelect;

  // Primary.
  const std::u16string last_primary_text = primary_label_->GetText();
  primary_label_->SetText(item()->GetText());
  primary_label_->SetEnabledColorId(selected() && multiselect
                                        ? kColorAshMultiSelectTextColor
                                        : kColorAshTextColorPrimary);

  // Secondary.
  const std::u16string last_secondary_text = secondary_label_->GetText();
  secondary_label_->SetText(
      item()->secondary_text().value_or(std::u16string()));

  if (selected() && multiselect) {
    secondary_label_->SetEnabledColorId(kColorAshMultiSelectTextColor);
  } else if (const std::optional<HoldingSpaceColorVariant>& color_variant =
                 item()->secondary_text_color_variant()) {
    // Handle the case where the `color_variant` is set.
    std::visit(base::Overloaded{
                   [&](const ui::ColorId& color_id) {
                     secondary_label_->SetEnabledColorId(color_id);
                   },
                   [&](const HoldingSpaceColors& colors) {
                     secondary_label_->SetEnabledColor(
                         DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
                             ? colors.dark_mode()
                             : colors.light_mode());
                   },
               },
               *color_variant);
  } else {
    // Use the default color.
    secondary_label_->SetEnabledColorId(kColorAshTextColorSecondary);
  }

  secondary_label_->SetVisible(!secondary_label_->GetText().empty());

  // Tooltip.
  // NOTE: Only necessary if the displayed text has changed.
  if (primary_label_->GetText() != last_primary_text ||
      secondary_label_->GetText() != last_secondary_text) {
    TooltipTextChanged();
  }
}

void HoldingSpaceItemChipView::UpdateSecondaryAction() {
  // If the associated `item()` has been deleted then `this` is in the process
  // of being destroyed and no action needs to be taken.
  if (!item())
    return;

  // NOTE: Only in-progress items currently support secondary actions.
  const bool has_secondary_action =
      !checkmark()->GetVisible() && !item()->progress().IsComplete() &&
      (holding_space_util::SupportsInProgressCommand(
           item(), HoldingSpaceCommandId::kPauseItem) ||
       holding_space_util::SupportsInProgressCommand(
           item(), HoldingSpaceCommandId::kResumeItem)) &&
      IsMouseHovered();

  if (!has_secondary_action) {
    secondary_action_container_->SetVisible(false);
    UpdateImageAndProgressIndicatorVisibility();
    return;
  }

  // Pause/resume.
  const bool is_item_paused = holding_space_util::SupportsInProgressCommand(
      item(), HoldingSpaceCommandId::kResumeItem);
  secondary_action_pause_->SetVisible(!is_item_paused);
  secondary_action_resume_->SetVisible(is_item_paused);

  secondary_action_container_->SetVisible(true);
  UpdateImageAndProgressIndicatorVisibility();
}

BEGIN_METADATA(HoldingSpaceItemChipView)
END_METADATA

}  // namespace ash
