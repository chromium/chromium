// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

#include <set>

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/bind.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(167);

// Helpers ---------------------------------------------------------------------

// Returns a callback which deletes the associated animation observer after
// running another `callback`.
using AnimationCompletedCallback = base::OnceCallback<void(bool aborted)>;
base::RepeatingCallback<bool(const ui::CallbackLayerAnimationObserver&)>
DeleteObserverAfterRunning(AnimationCompletedCallback callback) {
  return base::BindRepeating(
      [](AnimationCompletedCallback callback,
         const ui::CallbackLayerAnimationObserver& observer) {
        // NOTE: It's safe to move `callback` since this code will only run
        // once due to deletion of the associated `observer`. The `observer` is
        // deleted by returning `true`.
        std::move(callback).Run(/*aborted=*/observer.aborted_count() > 0);
        return true;
      },
      base::Passed(std::move(callback)));
}

// Returns whether the given holding space `model` contains any initialized
// items which are supported by the specified holding space item views
// `section`.
bool ModelContainsInitializedItemsForSection(
    const HoldingSpaceModel* model,
    const HoldingSpaceItemViewsSection* section) {
  const auto& supported_types = section->supported_types();
  return std::any_of(
      supported_types.begin(), supported_types.end(),
      [&model](HoldingSpaceItem::Type supported_type) {
        return model->ContainsInitializedItemOfType(supported_type);
      });
}

// TopAlignedBoxLayout ---------------------------------------------------------

// A vertical `views::BoxLayout` which overrides layout behavior when there is
// insufficient layout space to accommodate all children's preferred sizes.
// Unlike `views::BoxLayout` which will not allow children to exceed its content
// bounds, TopAlignedBoxLayout will ensure that children still receive their
// preferred sizes. This prevents layout jank that would otherwise occur when
// the host view's bounds are being animated due to content changes.
class TopAlignedBoxLayout : public views::BoxLayout {
 public:
  TopAlignedBoxLayout(const gfx::Insets& insets, int spacing)
      : views::BoxLayout(views::BoxLayout::Orientation::kVertical,
                         insets,
                         spacing) {}

 private:
  // views::BoxLayout:
  void Layout(views::View* host) override {
    if (host->height() >= host->GetPreferredSize().height()) {
      views::BoxLayout::Layout(host);
      return;
    }

    gfx::Rect contents_bounds(host->GetContentsBounds());
    contents_bounds.Inset(inside_border_insets());

    // If we only have a single child view and that child view is okay with
    // being sized arbitrarily small, short circuit layout logic and give that
    // child all available layout space. This is the case for the
    // `PinnedFileSection` which supports scrolling its content when necessary.
    if (host->children().size() == 1u &&
        host->children()[0]->GetMinimumSize().IsEmpty()) {
      host->children()[0]->SetBoundsRect(contents_bounds);
      return;
    }

    int top = contents_bounds.y();
    int left = contents_bounds.x();
    int width = contents_bounds.width();

    for (views::View* child : host->children()) {
      gfx::Size size(width, child->GetHeightForWidth(width));
      child->SetBounds(left, top, size.width(), size.height());
      top += size.height() + between_child_spacing();
    }
  }
};

}  // namespace

// HoldingSpaceTrayChildBubble -------------------------------------------------

HoldingSpaceTrayChildBubble::HoldingSpaceTrayChildBubble(
    HoldingSpaceViewDelegate* delegate)
    : delegate_(delegate) {
  controller_observer_.Observe(HoldingSpaceController::Get());
  if (HoldingSpaceController::Get()->model())
    model_observer_.Observe(HoldingSpaceController::Get()->model());
}

HoldingSpaceTrayChildBubble::~HoldingSpaceTrayChildBubble() = default;

void HoldingSpaceTrayChildBubble::Init() {
  // Layout.
  SetLayoutManager(std::make_unique<TopAlignedBoxLayout>(
      kHoldingSpaceChildBubblePadding, kHoldingSpaceChildBubbleChildSpacing));

  // Layer.
  // TODO(crbug/1313073): In dark light mode, since we have changed to use a
  // textured layer instead of a solid color layer, we need to remove all the
  // layer set up in the children of this view to remove layer redundancy.
  SetPaintToLayer(features::IsDarkLightModeEnabled() ? ui::LAYER_TEXTURED
                                                     : ui::LAYER_SOLID_COLOR);
  layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetOpacity(0.f);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kBubbleCornerRadius});

  // Sections.
  for (auto& section : CreateSections()) {
    sections_.push_back(AddChildView(std::move(section)));
    sections_.back()->Init();
  }
}

void HoldingSpaceTrayChildBubble::Reset() {
  // Prevent animation callbacks from running when the holding space bubble is
  // being asynchronously closed. This view will be imminently deleted.
  weak_factory_.InvalidateWeakPtrs();

  model_observer_.Reset();
  controller_observer_.Reset();

  for (HoldingSpaceItemViewsSection* section : sections_)
    section->Reset();
}

std::vector<HoldingSpaceItemView*>
HoldingSpaceTrayChildBubble::GetHoldingSpaceItemViews() {
  std::vector<HoldingSpaceItemView*> views;
  for (HoldingSpaceItemViewsSection* section : sections_) {
    auto section_views = section->GetHoldingSpaceItemViews();
    views.insert(views.end(), section_views.begin(), section_views.end());
  }
  return views;
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  model_observer_.Observe(model);

  // New model contents, if available, will be populated and animated in after
  // the out animation completes.
  MaybeAnimateOut();
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Reset();
  MaybeAnimateOut();
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  // Ignore new items while the bubble is animating out. The bubble content will
  // be updated to match the model after the out animation completes.
  if (!is_animating_out_) {
    for (HoldingSpaceItemViewsSection* section : sections_)
      section->OnHoldingSpaceItemsAdded(items);
  }
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  // Ignore item removal while the bubble is animating out. The bubble content
  // will be updated to match the model after the out animation completes.
  if (is_animating_out_)
    return;

  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  DCHECK(model);

  // This child bubble should animate out if the attached model does not
  // contain initialized items supported by any of its sections. The exception
  // is if a section has a placeholder to show in lieu of holding space items.
  // If a placeholder exists, the child bubble should persist.
  const bool animate_out = std::none_of(
      sections_.begin(), sections_.end(),
      [&model](const HoldingSpaceItemViewsSection* section) {
        return section->has_placeholder() ||
               ModelContainsInitializedItemsForSection(model, section);
      });

  if (animate_out) {
    MaybeAnimateOut();
    return;
  }

  for (HoldingSpaceItemViewsSection* section : sections_)
    section->OnHoldingSpaceItemsRemoved(items);
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {
  // Ignore item initialized while the bubble is animating out. The bubble
  // content will be updated to match the model after the out animation
  // completes.
  if (!is_animating_out_) {
    for (HoldingSpaceItemViewsSection* section : sections_)
      section->OnHoldingSpaceItemInitialized(item);
  }
}

const char* HoldingSpaceTrayChildBubble::GetClassName() const {
  return "HoldingSpaceTrayChildBubble";
}

void HoldingSpaceTrayChildBubble::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceTrayChildBubble::ChildVisibilityChanged(views::View* child) {
  if (ignore_child_visibility_changed_)
    return;

  // This child bubble should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children()) {
    if (c->GetVisible()) {
      visible = true;
      break;
    }
  }

  if (visible != GetVisible()) {
    SetVisible(visible);

    // When the child bubble becomes visible, its due to one of its sections
    // becoming visible. In this case, the child bubble should animate in.
    if (GetVisible())
      MaybeAnimateIn();
  }

  PreferredSizeChanged();
}

void HoldingSpaceTrayChildBubble::OnGestureEvent(ui::GestureEvent* event) {
  delegate_->OnHoldingSpaceTrayChildBubbleGestureEvent(*event);
  views::View::OnGestureEvent(event);
}

bool HoldingSpaceTrayChildBubble::OnMousePressed(const ui::MouseEvent& event) {
  delegate_->OnHoldingSpaceTrayChildBubbleMousePressed(event);
  return true;
}

void HoldingSpaceTrayChildBubble::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (!features::IsDarkLightModeEnabled()) {
    layer()->SetColor(AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80));
    return;
  }

  SetBackground(
      views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kBubbleCornerRadius, views::HighlightBorder::Type::kHighlightBorder1,
      /*use_light_colors=*/false));
}

void HoldingSpaceTrayChildBubble::MaybeAnimateIn() {
  // Don't preempt an out animation as new content will populate and be animated
  // in, if any exists, once the out animation completes.
  if (is_animating_out_)
    return;

  // Don't attempt to animate in this bubble unnecessarily as it will cause
  // opacity to revert to zero before proceeding to animate in. Ensure that
  // event processing is enabled as it may have been disabled while animating
  // this bubble out.
  if (layer()->GetTargetOpacity() == 1.f) {
    SetCanProcessEventsWithinSubtree(true);
    return;
  }

  // NOTE: `animate_in_observer` is deleted after `OnAnimateInCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_in_observer =
      new ui::CallbackLayerAnimationObserver(DeleteObserverAfterRunning(
          base::BindOnce(&HoldingSpaceTrayChildBubble::OnAnimateInCompleted,
                         weak_factory_.GetWeakPtr())));

  AnimateIn(animate_in_observer);
  animate_in_observer->SetActive();
}

void HoldingSpaceTrayChildBubble::MaybeAnimateOut() {
  if (is_animating_out_)
    return;

  // Child bubbles should not process events when being animated as the model
  // objects backing their views may no longer exist. Event processing will be
  // re-enabled on animation completion.
  SetCanProcessEventsWithinSubtree(false);

  // NOTE: `animate_out_observer` is deleted after `OnAnimateOutCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_out_observer =
      new ui::CallbackLayerAnimationObserver(DeleteObserverAfterRunning(
          base::BindOnce(&HoldingSpaceTrayChildBubble::OnAnimateOutCompleted,
                         weak_factory_.GetWeakPtr())));

  AnimateOut(animate_out_observer);
  animate_out_observer->SetActive();
}

void HoldingSpaceTrayChildBubble::AnimateIn(
    ui::LayerAnimationObserver* observer) {
  DCHECK(!is_animating_out_);

  // Animation is only necessary if this view is visible to the user.
  const base::TimeDelta animation_duration =
      IsDrawn() ? kAnimationDuration : base::TimeDelta();

  // Delay in animations to give the holding space bubble time to animate its
  // layout changes. This ensures that there is sufficient space to display the
  // child bubble before it is displayed to the user.
  const base::TimeDelta animation_delay =
      IsDrawn() ? kAnimationDuration : base::TimeDelta();
  holding_space_util::AnimateIn(this, animation_duration, animation_delay,
                                observer);
}

void HoldingSpaceTrayChildBubble::AnimateOut(
    ui::LayerAnimationObserver* observer) {
  DCHECK(!is_animating_out_);
  is_animating_out_ = true;

  // Animation is only necessary if this view is visible to the user.
  const base::TimeDelta animation_duration =
      IsDrawn() ? kAnimationDuration : base::TimeDelta();
  holding_space_util::AnimateOut(this, animation_duration, observer);
}

void HoldingSpaceTrayChildBubble::OnAnimateInCompleted(bool aborted) {
  // Restore event processing once the child bubble has fully animated in. Its
  // contents are guaranteed to exist in the model and can be acted upon by the
  // user.
  if (!aborted)
    SetCanProcessEventsWithinSubtree(true);
}

void HoldingSpaceTrayChildBubble::OnAnimateOutCompleted(bool aborted) {
  DCHECK(is_animating_out_);
  is_animating_out_ = false;

  if (aborted)
    return;

  // Once the child bubble has animated out it is transparent but still
  // "visible" as far as the views framework is concerned and so takes up layout
  // space. Hide the view so that the holding space bubble will animate the
  // re-layout of its view hierarchy with this child bubble taking no space.
  SetVisible(false);

  {
    // Removing all holding space items from a section will cause it to become
    // invisible. When multiple sections exist, the `ChildVisibilityChanged()`
    // event which results would normally cause this child bubble to regain
    // visibility since some sections have not yet been hidden. To prevent this
    // child bubble from becoming visible, ignore `ChildVisibilityChanged()`.
    base::AutoReset<bool> scoped_ignore_child_visibility_changed(
        &ignore_child_visibility_changed_, true);
    for (HoldingSpaceItemViewsSection* section : sections_)
      section->RemoveAllHoldingSpaceItemViews();
  }

  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (!model || model->items().empty())
    return;

  std::vector<const HoldingSpaceItem*> item_ptrs;
  for (const auto& item : model->items())
    item_ptrs.push_back(item.get());

  // Populating a `section` may cause it's visibility to change if the `model`
  // contains initialized items of types which it supports. This, in turn, will
  // cause visibility of this child bubble to update and animate in if needed.
  for (HoldingSpaceItemViewsSection* section : sections_)
    section->OnHoldingSpaceItemsAdded(item_ptrs);
}

}  // namespace ash
