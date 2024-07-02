// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_progress_indicator.h"

#include <algorithm>

#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kDotCount = 3;
constexpr float kDotLargeSizeDip = 6.f;
constexpr float kDotSmallSizeDip = 4.f;
constexpr int kDotSpacingDip = 3;
constexpr int kPreferredHeightDip = 9;

// Animation.
constexpr float kTranslationDip = -(kDotLargeSizeDip - kDotSmallSizeDip) / 2.f;
constexpr float kScaleFactor = kDotLargeSizeDip / kDotSmallSizeDip;

// Helpers ---------------------------------------------------------------------

bool AreAnimationsEnabled() {
  return ui::ScopedAnimationDurationScaleMode::duration_multiplier() !=
         ui::ScopedAnimationDurationScaleMode::ZERO_DURATION;
}

// DotBackground ---------------------------------------------------------------

class DotBackground : public views::Background {
 public:
  DotBackground() = default;

  DotBackground(const DotBackground&) = delete;
  DotBackground& operator=(const DotBackground&) = delete;

  ~DotBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey300);

    const gfx::Rect& b = view->GetContentsBounds();
    const gfx::Point center = gfx::Point(b.width() / 2, b.height() / 2);
    const int radius = std::min(b.width() / 2, b.height() / 2);

    canvas->DrawCircle(center, radius, flags);
  }
};

}  // namespace

// AssistantProgressIndicator --------------------------------------------------

AssistantProgressIndicator::AssistantProgressIndicator() {
  SetID(AssistantViewID::kProgressIndicator);
  InitLayout();
}

AssistantProgressIndicator::~AssistantProgressIndicator() = default;

gfx::Size AssistantProgressIndicator::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int preferred_width =
      views::View::CalculatePreferredSize(available_size).width();
  return gfx::Size(preferred_width, kPreferredHeightDip);
}

void AssistantProgressIndicator::AddedToWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/GetVisible());
}

void AssistantProgressIndicator::RemovedFromWidget() {
  VisibilityChanged(/*starting_from=*/this, /*is_visible=*/false);
}

void AssistantProgressIndicator::OnLayerOpacityChanged(
    ui::PropertyChangeReason reason) {
  VisibilityChanged(/*starting_from=*/this,
                    /*is_visible=*/GetVisible());
}

void AssistantProgressIndicator::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  // Stop the animation when the view is either not visible or is "visible" but
  // not actually visible to the user (because it is faded out).
  const bool is_drawn =
      IsDrawn() && !cc::MathUtil::IsWithinEpsilon(layer()->opacity(), 0.f);
  if (is_drawn_ == is_drawn)
    return;

  is_drawn_ = is_drawn;

  if (!is_drawn_) {
    // Stop all animations.
    for (views::View* child : children()) {
      child->layer()->GetAnimator()->StopAnimating();
    }
    return;
  }

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateTransformElement;

  // The animation performs scaling on the child views. In order to give the
  // illusion that scaling is being performed about the center of the view as
  // the transformation origin, we also need to perform a translation.
  gfx::Transform transform;
  transform.Translate(kTranslationDip, kTranslationDip);
  transform.Scale(kScaleFactor, kScaleFactor);

  // Don't animate if animations are disabled (during unittests).
  // Otherwise we get in an infinite loop due to the cyclic animation used here
  // repeating over and over without pause.
  if (!AreAnimationsEnabled())
    return;

  base::TimeDelta start_offset;
  for (views::View* child : children()) {
    if (!start_offset.is_zero()) {
      // Schedule the animations to start after an offset.
      child->layer()->GetAnimator()->SchedulePauseForProperties(
          start_offset,
          ui::LayerAnimationElement::AnimatableProperty::TRANSFORM);
    }
    start_offset += base::Milliseconds(216);

    // Schedule transformation animation.
    child->layer()->GetAnimator()->ScheduleAnimation(
        CreateLayerAnimationSequence(
            // Animate scale up.
            CreateTransformElement(transform, base::Milliseconds(266)),
            // Animate scale down.
            CreateTransformElement(gfx::Transform(), base::Milliseconds(450)),
            // Pause before next iteration.
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::TRANSFORM,
                base::Milliseconds(500)),
            // Animation parameters.
            {/*is_cyclic=*/true}));
  }
}

void AssistantProgressIndicator::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kDotSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Initialize dots.
  for (int i = 0; i < kDotCount; ++i) {
    auto dot_view = std::make_unique<views::View>();
    dot_view->SetBackground(std::make_unique<DotBackground>());
    dot_view->SetPreferredSize(gfx::Size(kDotSmallSizeDip, kDotSmallSizeDip));

    // Dots will animate on their own layers.
    dot_view->SetPaintToLayer();
    dot_view->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(std::move(dot_view));
  }
}

BEGIN_METADATA(AssistantProgressIndicator)
END_METADATA

}  // namespace ash
