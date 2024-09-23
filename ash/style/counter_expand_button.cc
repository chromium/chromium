// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/counter_expand_button.h"

#include <string>

#include "ash/public/cpp/metrics_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr gfx::Insets kFocusInsets(2);
constexpr gfx::Insets kImageInsets(2);
constexpr auto kLabelInsets = gfx::Insets::TLBR(0, 8, 0, 0);
constexpr int kCornerRadius = 12;
constexpr int kChevronIconSize = 16;
constexpr int kJellyChevronIconSize = 20;
constexpr int kLabelFontSize = 12;

}  // namespace

CounterExpandButton::CounterExpandButton() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  auto label = std::make_unique<views::Label>();
  label->SetPaintToLayer();
  label->layer()->SetFillsBoundsOpaquely(false);
  label->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                   kLabelFontSize, gfx::Font::Weight::MEDIUM));

  label->SetProperty(views::kMarginsKey, kLabelInsets);
  label->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
  label->SetText(base::NumberToString16(counter_));
  label->SetVisible(ShouldShowLabel());
  label_ = AddChildView(std::move(label));
  if (chromeos::features::IsJellyEnabled()) {
    ash::TypographyProvider::Get()->StyleLabel(
        ash::TypographyToken::kCrosAnnotation1, *label_);
  }

  auto image = std::make_unique<views::ImageView>();
  image->SetPaintToLayer();
  image->layer()->SetFillsBoundsOpaquely(false);
  image->SetProperty(views::kMarginsKey, kImageInsets);
  image_ = AddChildView(std::move(image));

  UpdateTooltip();

  views::InstallRoundRectHighlightPathGenerator(this, kFocusInsets,
                                                kCornerRadius);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);

  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kTrayItemCornerRadius});
  layer()->SetIsFastRoundedCorner(true);
}

CounterExpandButton::~CounterExpandButton() = default;

void CounterExpandButton::SetExpanded(bool expanded) {
  if (expanded_ == expanded) {
    return;
  }

  previous_bounds_ = GetContentsBounds();

  expanded_ = expanded;

  label_->SetText(base::NumberToString16(counter_));
  label_->SetVisible(ShouldShowLabel());

  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);

  UpdateTooltip();
}

bool CounterExpandButton::ShouldShowLabel() const {
  return !expanded_ && counter_;
}

void CounterExpandButton::UpdateCounter(int count) {
  counter_ = count;
  label_->SetText(base::NumberToString16(counter_));
  label_->SetVisible(ShouldShowLabel());
}

void CounterExpandButton::UpdateIcons() {
  SkColor icon_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface);
  int icon_size = chromeos::features::IsJellyEnabled() ? kJellyChevronIconSize
                                                       : kChevronIconSize;

  expanded_image_ =
      gfx::CreateVectorIcon(kChevronUpSmallIcon, icon_size, icon_color);

  collapsed_image_ =
      gfx::CreateVectorIcon(kChevronDownSmallIcon, icon_size, icon_color);

  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
}

void CounterExpandButton::UpdateTooltip() {
  std::u16string tooltip_text = expanded_ ? GetExpandedStateTooltipText()
                                          : GetCollapsedStateTooltipText();
  SetTooltipText(tooltip_text);
  GetViewAccessibility().SetName(
      tooltip_text, tooltip_text.empty()
                        ? ax::mojom::NameFrom::kAttributeExplicitlyEmpty
                        : ax::mojom::NameFrom::kAttribute);
}

void CounterExpandButton::AnimateExpandCollapse() {
  // If there is no child to expand/collapse, there's no animation to perform
  // here.
  if (!counter_) {
    return;
  }

  int bounds_animation_duration;
  gfx::Tween::Type bounds_animation_tween_type;

  if (label()->GetVisible()) {
    if (label()->layer()->GetAnimator()->is_animating()) {
      // Label's fade out animation might still be running. If that's the case,
      // we need to abort this and reset visibility for fade in animation.
      label()->layer()->GetAnimator()->AbortAllAnimations();
      label()->SetVisible(true);
    }

    // Fade in animation when label is visible.
    // TODO(b/336646488): Move `message_center_utils` functions and variables
    // used in this file to ash/style.
    message_center_utils::FadeInView(
        label(), kExpandButtonFadeInLabelDelayMs,
        kExpandButtonFadeInLabelDurationMs, gfx::Tween::LINEAR,
        GetAnimationHistogramName(AnimationType::kFadeInLabel));

    bounds_animation_duration = kExpandButtonShowLabelBoundsChangeDurationMs;
    bounds_animation_tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
  } else {
    // In this case, `counter_` is not zero and label is not visible.
    // This means the label switch from visible to invisible and we should do
    // fade out animation.
    label_fading_out_ = true;
    // TODO(b/336646488): Move `message_center_utils` functions and variables
    // used in this file to ash/style.
    message_center_utils::FadeOutView(
        label(),
        base::BindRepeating(
            [](base::WeakPtr<CounterExpandButton> parent, views::Label* label) {
              if (parent) {
                label->layer()->SetOpacity(1.0f);
                label->SetVisible(false);
                parent->set_label_fading_out(false);
              }
            },
            weak_factory_.GetWeakPtr(), label()),
        0, kExpandButtonFadeOutLabelDurationMs, gfx::Tween::LINEAR,
        GetAnimationHistogramName(AnimationType::kFadeOutLabel));

    bounds_animation_duration = kExpandButtonHideLabelBoundsChangeDurationMs;
    bounds_animation_tween_type = gfx::Tween::ACCEL_20_DECEL_100;
  }

  AnimateBoundsChange(bounds_animation_duration, bounds_animation_tween_type,
                      GetAnimationHistogramName(AnimationType::kBoundsChange));
}

const std::string CounterExpandButton::GetAnimationHistogramName(
    AnimationType type) {
  return "";
}

void CounterExpandButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  UpdateIcons();
  UpdateBackgroundColor();
}

gfx::Size CounterExpandButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = Button::CalculatePreferredSize(available_size);

  // When label is fading out, it is still visible but we should not consider
  // its size in our calculation here, so that size change animation can be
  // performed correctly.
  if (label_fading_out_) {
    return gfx::Size(
        size.width() -
            label_->GetPreferredSize(views::SizeBounds(label_->width(), {}))
                .width() -
            kLabelInsets.width(),
        size.height());
  }

  return size;
}

void CounterExpandButton::AnimateBoundsChange(
    int duration_in_ms,
    gfx::Tween::Type tween_type,
    const std::string& animation_histogram_name) {
  // Perform size change animation with layer bounds animation, setting the
  // bounds to its previous state and then animating to current state. At the
  // same time, we move `image_` in the opposite direction so that it appears to
  // stay in the same location when the parent's bounds is moving.
  const gfx::Rect target_bounds = layer()->GetTargetBounds();
  const gfx::Rect image_target_bounds = image_->layer()->GetTargetBounds();

  // This value is used to add extra width to the view's bounds. We will animate
  // the view with this extra width to its target state.
  int extra_width = previous_bounds_.width() - target_bounds.width();

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating(
          [](const std::string& animation_histogram_name, int smoothness) {
            base::UmaHistogramPercentage(animation_histogram_name, smoothness);
          },
          animation_histogram_name)));

  layer()->SetBounds(
      gfx::Rect(target_bounds.x() - extra_width, target_bounds.y(),
                target_bounds.width() + extra_width, target_bounds.height()));
  image_->layer()->SetBounds(
      gfx::Rect(image_target_bounds.x() + extra_width, image_target_bounds.y(),
                image_target_bounds.width(), image_target_bounds.height()));

  views::AnimationBuilder()
      .Once()
      .SetDuration(base::Milliseconds(duration_in_ms))
      .SetBounds(this, target_bounds, tween_type)
      .SetBounds(image_, image_target_bounds, tween_type);
}

std::u16string CounterExpandButton::GetExpandedStateTooltipText() const {
  return u"";
}

std::u16string CounterExpandButton::GetCollapsedStateTooltipText() const {
  return u"";
}

void CounterExpandButton::UpdateBackgroundColor() {
  layer()->SetColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase1));
}

BEGIN_METADATA(CounterExpandButton)
END_METADATA

}  // namespace ash
