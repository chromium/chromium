// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_search_page.h"

#include <memory>

#include "ash/app_list/views/app_list_search_view.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// The animation spec says 40 dips up over 250ms, but the opacity animation
// renders the view invisible after 50ms, so animate the visible fraction.
constexpr int kHideAnimationVerticalOffset = -40 * 50 / 250;

// Duration for the hide animation (both transform and opacity).
constexpr base::TimeDelta kHideAnimationDuration = base::Milliseconds(50);

constexpr auto kSearchViewBorder =
    gfx::Insets::TLBR(0, 0, kUpdatedBubbleCornerRadius, 0);
constexpr auto kDeprecatedSearchViewBorder =
    gfx::Insets::TLBR(0, 0, kDeprecatedBubbleCornerRadius, 0);
}  // namespace

AppListBubbleSearchPage::AppListBubbleSearchPage(
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchBoxView* search_box_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  search_view_ = AddChildView(std::make_unique<AppListSearchView>(
      view_delegate, dialog_controller, search_box_view));
  search_view_->SetBorder(
      views::CreateEmptyBorder(features::IsBubbleCornerRadiusUpdateEnabled()
                                   ? kSearchViewBorder
                                   : kDeprecatedSearchViewBorder));
}

AppListBubbleSearchPage::~AppListBubbleSearchPage() = default;

void AppListBubbleSearchPage::AnimateShowPage() {
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

  ui::Layer* layer = search_view_->GetPageAnimationLayer();
  DCHECK_EQ(layer->type(), ui::LAYER_TEXTURED);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(layer, 0.f)
      .At(base::Milliseconds(50))
      .SetDuration(base::Milliseconds(100))
      .SetOpacity(layer, 1.f);
}

void AppListBubbleSearchPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  // Update view visibility when the animation is done.
  auto set_visible_false = base::BindRepeating(
      [](base::WeakPtr<AppListBubbleSearchPage> self) {
        if (!self)
          return;
        self->SetVisible(false);
        ui::Layer* layer = self->search_view_->GetPageAnimationLayer();
        layer->SetOpacity(1.f);
        layer->SetTransform(gfx::Transform());
      },
      weak_factory_.GetWeakPtr());

  ui::Layer* layer = search_view_->GetPageAnimationLayer();
  DCHECK_EQ(layer->type(), ui::LAYER_TEXTURED);

  gfx::Transform translate_up;
  translate_up.Translate(0, kHideAnimationVerticalOffset);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(set_visible_false)
      .OnAborted(set_visible_false)
      .Once()
      .SetDuration(kHideAnimationDuration)
      .SetOpacity(layer, 0.f)
      .SetTransform(layer, translate_up);
}

void AppListBubbleSearchPage::AbortAllAnimations() {
  search_view_->GetPageAnimationLayer()->GetAnimator()->AbortAllAnimations();
}

ui::Layer* AppListBubbleSearchPage::GetPageAnimationLayerForTest() {
  return search_view_->GetPageAnimationLayer();
}

BEGIN_METADATA(AppListBubbleSearchPage)
END_METADATA

}  // namespace ash
