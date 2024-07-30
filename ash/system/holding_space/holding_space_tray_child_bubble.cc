// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_child_bubble.h"

#include <set>

#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager_base.h"

namespace ash {

namespace {

// Animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(167);

// Helpers ---------------------------------------------------------------------

// Returns a callback which deletes the associated animation observer after
// running another `callback` by returning true. This workaround is needed
// because callbacks that bind to a WeakPtr receiver cannot return a non-void
// type.
//
// TODO(crbug.com/40947532): It would be nice if CallbackLayerAnimationObserver
// took a OnceCallback and used that as an implicit signal to self-delete the
// observer on completion. Until then, this needs to use a RepeatingCallback,
// even though the callback only runs once.
using AnimationCompletedCallback = base::RepeatingCallback<void(bool aborted)>;
base::RepeatingCallback<bool(const ui::CallbackLayerAnimationObserver&)>
DeleteObserverAfterRunning(AnimationCompletedCallback callback) {
  return base::BindRepeating(
      [](const AnimationCompletedCallback& callback,
         const ui::CallbackLayerAnimationObserver& observer) {
        callback.Run(/*aborted=*/observer.aborted_count() > 0);
        // Returning true is load-bearing; when returning true, the observer
        // self-deletes so this callback will only ever run at most once.
        return true;
      },
      std::move(callback));
}

// Returns whether the given holding space item views `section` has content
// based on whether the active holding space model contains any initialized
// items which are supported by the `section`. If `section` has a placeholder
// it will always have content.
bool HasContentForSection(const HoldingSpaceItemViewsSection* section) {
  if (section->has_placeholder())
    return true;

  const auto* model = HoldingSpaceController::Get()->model();
  if (!model)
    return false;

  return base::ranges::any_of(
      section->supported_types(),
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
class TopAlignedBoxLayout : public views::LayoutManagerBase {
 public:
  TopAlignedBoxLayout(const gfx::Insets& insets, int spacing) {
    box_layout_ = AddOwnedLayout(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, insets, spacing));
  }

 private:
  // views::LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    if (!size_bounds.is_fully_bounded() ||
        size_bounds.height().value() >=
            host_view()->GetPreferredSize({}).height()) {
      return box_layout_->GetProposedLayout(size_bounds, PassKey());
    }

    views::ProposedLayout layout;
    layout.host_size =
        gfx::Size(size_bounds.width().value(), size_bounds.height().value());

    gfx::Rect contents_bounds(gfx::Point(0, 0), layout.host_size);
    contents_bounds.Inset(host_view()->GetInsets() +
                          box_layout_->inside_border_insets());

    const int width = contents_bounds.width();
    const int child_spacing = box_layout_->between_child_spacing();

    std::vector<std::pair<views::View*, int>> children_with_heights;

    // Calculate preferred heights for children at `width`. Note that
    // `available_height` is tracked to later determine if there will be
    // vertical overflow of `contents_bounds`.
    int available_height = contents_bounds.height();
    for (views::View* child : host_view()->children()) {
      if (!child->GetVisible()) {
        continue;
      }

      if (!children_with_heights.empty()) {
        available_height -= child_spacing;
      }

      const int preferred_height = child->GetHeightForWidth(width);
      children_with_heights.emplace_back(child, preferred_height);

      available_height -= preferred_height;
    }

    int top = contents_bounds.y();
    const int left = contents_bounds.x();

    // Perform child layouts, ceding height where possible to fit within
    // `contents_bounds`. Note: this does not guarantee that `contents_bounds`
    // will not be exceeded. Overflow will be clipped by the `host_view()` view.
    for (auto& [child, height] : children_with_heights) {
      // A `child` view is willing to cede height if it does not specify a
      // minimum size. This is the case for the `PinnedFilesSection` which
      // supports scrolling its content when necessary. In the future it may be
      // worth implementing a more equitable ceding strategy, but for now height
      // is greedily taken from children in order of appearance.
      if (available_height < 0 && child->GetMinimumSize().IsEmpty()) {
        const int ceded_height = std::min(height, -available_height);
        height -= ceded_height;
        available_height += ceded_height;
      }

      if (top > contents_bounds.y()) {
        top += child_spacing;
      }

      layout.child_layouts.emplace_back(child, true,
                                        gfx::Rect(left, top, width, height));

      top += height;
    }

    return layout;
  }

  raw_ptr<views::BoxLayout> box_layout_;
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
  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.f);

  // Child bubbles should mask child layers to bounds so as not to paint over
  // other child bubbles in the event of overflow.
  layer()->SetMasksToBounds(true);

  // Background.
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetBackground(views::CreateThemedSolidBackground(
      chromeos::features::IsJellyEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)
          : kColorAshShieldAndBase80));

  // Border.
  const float corner_radius = GetBubbleCornerRadius();
  SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius,
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderOnShadow
          : views::HighlightBorder::Type::kHighlightBorder1));

  // Corner radius.
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{corner_radius});

  // Placeholder.
  if (auto placeholder = CreatePlaceholder()) {
    placeholder_ = AddChildView(std::move(placeholder));
    layer()->SetOpacity(1.f);
  }

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
  if (is_animating_out_)
    return;

  // This child bubble should animate out if it was previously showing its
  // `placeholder_` but will now transition to showing one or more `sections_`.
  const bool animate_out =
      placeholder_ && placeholder_->GetVisible() &&
      base::ranges::any_of(sections_, &HasContentForSection);

  if (animate_out) {
    MaybeAnimateOut();
    return;
  }

  for (HoldingSpaceItemViewsSection* section : sections_)
    section->OnHoldingSpaceItemsAdded(items);
}

void HoldingSpaceTrayChildBubble::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  // Ignore item removal while the bubble is animating out. The bubble content
  // will be updated to match the model after the out animation completes.
  if (is_animating_out_)
    return;

  // This child bubble should animate out if it does not have a visible
  // `placeholder_` and will not be showing one or more `sections_`.
  const bool animate_out =
      (!placeholder_ || !placeholder_->GetVisible()) &&
      base::ranges::none_of(sections_, &HasContentForSection);

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
  if (is_animating_out_)
    return;

  // This child bubble should animate out if it was previously showing its
  // `placeholder_` but will now transition to showing one or more `sections_`.
  const bool animate_out =
      placeholder_ && placeholder_->GetVisible() &&
      base::ranges::any_of(sections_, &HasContentForSection);

  if (animate_out) {
    MaybeAnimateOut();
    return;
  }

  for (HoldingSpaceItemViewsSection* section : sections_)
    section->OnHoldingSpaceItemInitialized(item);
}

std::unique_ptr<views::View> HoldingSpaceTrayChildBubble::CreatePlaceholder() {
  return nullptr;
}

void HoldingSpaceTrayChildBubble::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceTrayChildBubble::ChildVisibilityChanged(views::View* child) {
  if (ignore_child_visibility_changed_)
    return;

  // This child bubble should be visible iff it has visible children. Note that
  // if the child bubble has a placeholder, it will always be visible.
  const bool visible =
      placeholder_ ||
      base::ranges::any_of(children(), [](const views::View* child) {
        return child->GetVisible();
      });

  if (placeholder_) {
    // The `placeholder_` should only be visible if all `sections_` are not.
    // Note that `ChildVisibilityChanged()` events are suppressed here for
    // `placeholder_` to prevent nested visibility changed events.
    base::AutoReset reset(&ignore_child_visibility_changed_, true);
    placeholder_->SetVisible(base::ranges::none_of(
        sections_, [](const HoldingSpaceItemViewsSection* section) {
          return section->GetVisible();
        }));
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
      new ui::CallbackLayerAnimationObserver(
          DeleteObserverAfterRunning(base::BindRepeating(
              &HoldingSpaceTrayChildBubble::OnAnimateInCompleted,
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
      new ui::CallbackLayerAnimationObserver(
          DeleteObserverAfterRunning(base::BindRepeating(
              &HoldingSpaceTrayChildBubble::OnAnimateOutCompleted,
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
  if (!model)
    return;

  std::vector<const HoldingSpaceItem*> item_ptrs;
  for (const auto& item : model->items())
    item_ptrs.push_back(item.get());

  // Populating a `section` may cause it's visibility to change if the `model`
  // contains initialized items of types which it supports. This, in turn, will
  // cause visibility of this child bubble to update and animate in if needed.
  if (!item_ptrs.empty()) {
    for (HoldingSpaceItemViewsSection* section : sections_)
      section->OnHoldingSpaceItemsAdded(item_ptrs);
  }

  if (placeholder_) {
    // The `placeholder_` should only be visible if all `sections_` are not.
    placeholder_->SetVisible(base::ranges::none_of(
        sections_, [](const HoldingSpaceItemViewsSection* section) {
          return section->GetVisible();
        }));
  }
}

BEGIN_METADATA(HoldingSpaceTrayChildBubble)
END_METADATA

}  // namespace ash
