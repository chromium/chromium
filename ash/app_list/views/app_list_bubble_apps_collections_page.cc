// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"

#include <limits>
#include <memory>
#include <utility>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;
namespace ash {

namespace {

// Insets for the vertical scroll bar. The bottom is pushed up slightly to keep
// the scroll bar from being clipped by the rounded corners.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(1, 0, 16, 1);

// The padding between different sections within the apps collections page. Also
// used for interior page container margin.
constexpr int kVerticalPaddingBetweenSections = 16;

// The horizontal interior margin for the apps page container - i.e. the margin
// between the page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 16;

// TODO(anasalazar): Update the animation details when a motion spec is set.
// Right now we are using the same transition as the apps page. The spec says
// "Down 40 -> 0, duration 250ms" with no delay, but the opacity animation has a
// 50ms delay that causes the first 50ms to be invisible. Just animate the 200ms
// visible part, which is 32 dips. This ensures the search page hide animation
// doesn't play at the same time as the apps page show animation.
constexpr int kShowPageAnimationVerticalOffset = 32;
constexpr base::TimeDelta kShowPageAnimationTransformDuration =
    base::Milliseconds(200);

// Delay for the show page transform and opacity animations.
constexpr base::TimeDelta kShowPageAnimationDelay = base::Milliseconds(50);

// Duration of the show page opacity animation.
constexpr base::TimeDelta kShowPageAnimationOpacityDuration =
    base::Milliseconds(100);

}  // namespace

AppListBubbleAppsCollectionsPage::AppListBubbleAppsCollectionsPage() {
  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(std::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      RoundedScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_bar_ = vertical_scroll.get();
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout = scroll_contents->SetLayoutManager(std::make_unique<BoxLayout>(
      BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalPaddingBetweenSections,
                      kHorizontalInteriorMargin),
      kVerticalPaddingBetweenSections));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  scroll_view_->SetContents(std::move(scroll_contents));
}

AppListBubbleAppsCollectionsPage::~AppListBubbleAppsCollectionsPage() = default;

void AppListBubbleAppsCollectionsPage::AnimateShowPage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(true);
    return;
  }

  // Ensure any in-progress animations have their cleanup callbacks called.
  // Note that this might call SetVisible(false) from the hide animation.
  AbortAllAnimations();

  // Ensure the view is visible.
  SetVisible(true);

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  gfx::Transform translate_down;
  translate_down.Translate(0, kShowPageAnimationVerticalOffset);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ true))
      .OnAborted(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ true))
      .Once()
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationTransformDuration)
      .SetTransform(scroll_contents, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationOpacityDuration)
      .SetOpacity(scroll_contents, 1.f);
}

void AppListBubbleAppsCollectionsPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  // Scroll contents has a layer, so animate that.
  views::View* scroll_contents = scroll_view_->contents();
  DCHECK(scroll_contents->layer());
  DCHECK_EQ(scroll_contents->layer()->type(), ui::LAYER_TEXTURED);

  // The animation spec says 40 dips down over 250ms, but the opacity animation
  // renders the view invisible after 50ms, so animate the visible fraction.
  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40 * 250 / 50;
  translate_down.Translate(0, kVerticalOffset);

  // Opacity: 100% -> 0%, duration 50ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ false))
      .OnAborted(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ false))
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(scroll_contents, 0.f)
      .SetTransform(scroll_contents, translate_down);
}

void AppListBubbleAppsCollectionsPage::AbortAllAnimations() {
  if (scroll_view_->contents()->layer()) {
    scroll_view_->contents()->layer()->GetAnimator()->AbortAllAnimations();
  }
}

ui::Layer* AppListBubbleAppsCollectionsPage::GetPageAnimationLayerForTest() {
  return scroll_view_->contents()->layer();
}

void AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation(
    bool visible) {
  // Ensure the view has the correct opacity and transform when the animation is
  // aborted.
  SetVisible(visible);
  ui::Layer* layer = scroll_view()->contents()->layer();
  layer->SetOpacity(1.f);
  layer->SetTransform(gfx::Transform());
}

BEGIN_METADATA(AppListBubbleAppsCollectionsPage)
END_METADATA

}  // namespace ash
