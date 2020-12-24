// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_section.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "base/auto_reset.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/platform_style.h"

namespace ash {

namespace {

// Animation.
constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr SkScalar kAnimationTranslationY = 20;

// Value returned during notification of animation completion events in order to
// delete the observer which provided notification.
constexpr bool kDeleteObserver = true;

// Helpers ---------------------------------------------------------------------

// Initializes the layer for the specified `view` for animations.
void InitLayerForAnimations(views::View* view) {
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
  view->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Creates a `ui::LayerAnimationSequence` for the specified `element` observed
// by the specified `observer`.
std::unique_ptr<ui::LayerAnimationSequence> CreateObservedSequence(
    std::unique_ptr<ui::LayerAnimationElement> element,
    ui::LayerAnimationObserver* observer) {
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(std::move(element));
  sequence->AddObserver(observer);
  return sequence;
}

// Creates a `gfx:Transform` for the specified `x` and `y` offsets.
gfx::Transform CreateTransformFromOffset(SkScalar x, SkScalar y) {
  gfx::Transform transform;
  transform.Translate(x, y);
  return transform;
}

// Animates the specified `view` to a target `opacity` and `transform` with the
// specified `duration`, associating `observer` with the created animation
// sequences.
void DoAnimateTo(views::View* view,
                 float opacity,
                 const gfx::Transform& transform,
                 base::TimeDelta duration,
                 ui::LayerAnimationObserver* observer) {
  // Opacity animation.
  auto opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);

  // Transform animation.
  auto transform_element =
      ui::LayerAnimationElement::CreateTransformElement(transform, duration);
  transform_element->set_tween_type(gfx::Tween::Type::EASE_OUT_3);

  // Note that the `ui::LayerAnimator` takes ownership of any animation
  // sequences so they need to be released.
  view->layer()->GetAnimator()->StartTogether(
      {CreateObservedSequence(std::move(opacity_element), observer).release(),
       CreateObservedSequence(std::move(transform_element), observer)
           .release()});
}

// Animates in the specified `view` with the specified `duration`, associating
// `observer` with the created animation sequences.
void DoAnimateIn(views::View* view,
                 base::TimeDelta duration,
                 ui::LayerAnimationObserver* observer) {
  view->layer()->SetOpacity(0.f);
  view->layer()->SetTransform(
      CreateTransformFromOffset(0, kAnimationTranslationY));
  DoAnimateTo(view, /*opacity=*/1.f, gfx::Transform(), duration, observer);
}

// Animates out the specified `view` with the specified `duration, associating
// `observer` with the created animation sequences.
void DoAnimateOut(views::View* view,
                  base::TimeDelta duration,
                  ui::LayerAnimationObserver* observer) {
  DoAnimateTo(view, /*opacity=*/0.f,
              CreateTransformFromOffset(0, -kAnimationTranslationY), duration,
              observer);
}

// HoldingSpaceScrollView ------------------------------------------------------

class HoldingSpaceScrollView : public views::ScrollView,
                               public views::ViewObserver {
 public:
  HoldingSpaceScrollView() {
    // `HoldingSpaceItemView`s draw a focus ring outside of their view bounds.
    // `HoldingSpaceScrollView` needs to paint to a layer to avoid clipping
    // these focus rings.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  views::View* SetContents(std::unique_ptr<views::View> view) {
    views::View* contents = views::ScrollView::SetContents(std::move(view));
    view_observer_.Observe(contents);
    return contents;
  }

 private:
  // views::ScrollView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    // The focus ring for `HoldingSpaceItemView`s is painted just outside of
    // their view bounds. The clip rect for this view should be expanded to
    // avoid clipping of these focus rings. Note that a clip rect *does* need to
    // be applied to prevent this view from painting its contents outside of its
    // viewport.
    const float kFocusInsets =
        kHoldingSpaceFocusInsets -
        (views::PlatformStyle::kFocusHaloThickness / 2.f);
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(gfx::Insets(kFocusInsets));
    layer()->SetClipRect(bounds);
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(View* observed_view) override {
    PreferredSizeChanged();
  }

  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override {
    // Sync scroll view visibility with contents visibility.
    if (GetVisible() != observed_view->GetVisible())
      SetVisible(observed_view->GetVisible());
  }

  void OnViewIsDeleting(View* observed_view) override {
    view_observer_.Reset();
  }

  base::ScopedObservation<views::View, views::ViewObserver> view_observer_{
      this};
};

}  // namespace

// HoldingSpaceItemViewsSection ------------------------------------------------

HoldingSpaceItemViewsSection::HoldingSpaceItemViewsSection(
    HoldingSpaceItemViewDelegate* delegate,
    std::vector<HoldingSpaceItem::Type> supported_types,
    const base::Optional<size_t>& max_count)
    : delegate_(delegate),
      supported_types_(std::move(supported_types)),
      max_count_(max_count) {
  controller_observer_.Observe(HoldingSpaceController::Get());
}

HoldingSpaceItemViewsSection::~HoldingSpaceItemViewsSection() = default;

void HoldingSpaceItemViewsSection::Init() {
  SetVisible(false);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHoldingSpaceSectionChildSpacing));

  // Header.
  header_ = AddChildView(CreateHeader());
  header_->SetVisible(false);

  // Container.
  // NOTE: If `max_count_` is not present `container_` does not limit the number
  // of holding space item views visible to the user at one time. In this case
  // `container_` needs to be wrapped in a `views::ScrollView` to allow the user
  // access to all contained item views.
  if (max_count_.has_value()) {
    container_ = AddChildView(CreateContainer());
  } else {
    auto* scroll = AddChildView(std::make_unique<HoldingSpaceScrollView>());
    scroll->SetBackgroundColor(base::nullopt);
    scroll->ClipHeightTo(0, INT_MAX);
    scroll->SetDrawOverflowIndicator(false);
    container_ = scroll->SetContents(CreateContainer());
  }

  InitLayerForAnimations(container_);
  container_->SetVisible(false);

  // Placeholder.
  auto placeholder = CreatePlaceholder();
  if (placeholder) {
    placeholder_ = AddChildView(std::move(placeholder));
    InitLayerForAnimations(placeholder_);
    placeholder_->SetVisible(true);
    header_->SetVisible(true);
  }

  // Views.
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (model) {
    // Sections are not animated during initialization as their respective
    // bubbles will be animated in instead.
    base::AutoReset<bool> scoped_disable_animations(&disable_animations_, true);
    OnHoldingSpaceModelAttached(model);
  }
}

void HoldingSpaceItemViewsSection::Reset() {
  model_observer_.Reset();
  controller_observer_.Reset();
}

void HoldingSpaceItemViewsSection::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::ChildVisibilityChanged(views::View* child) {
  // This section should be visible iff it has visible children.
  bool visible = false;
  for (const views::View* c : children()) {
    if (c->GetVisible()) {
      visible = true;
      break;
    }
  }

  if (visible != GetVisible())
    SetVisible(visible);

  PreferredSizeChanged();
}

void HoldingSpaceItemViewsSection::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.parent != container_)
    return;

  // Update visibility when becoming empty or non-empty. Note that in the case
  // of a view being added, `ViewHierarchyChanged()` is called *after* the view
  // has been parented but in the case of a view being removed, it is called
  // *before* the view is un-parented.
  if (container_->children().size() != 1u)
    return;

  header_->SetVisible(placeholder_ || details.is_add);
  container_->SetVisible(details.is_add);

  if (placeholder_)
    placeholder_->SetVisible(!details.is_add);
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  model_observer_.Observe(model);

  std::vector<const HoldingSpaceItem*> item_ptrs;
  for (const auto& item : model->items())
    item_ptrs.push_back(item.get());

  if (!item_ptrs.empty())
    OnHoldingSpaceItemsAdded(item_ptrs);
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Reset();
  if (!container_->children().empty())
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {
  const bool needs_update = std::any_of(
      items.begin(), items.end(), [this](const HoldingSpaceItem* item) {
        return item->IsFinalized() &&
               base::Contains(supported_types_, item->type());
      });
  if (needs_update)
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {
  const bool needs_update = std::any_of(
      items.begin(), items.end(), [this](const HoldingSpaceItem* item) {
        return base::Contains(views_by_item_id_, item->id());
      });
  if (needs_update)
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  if (base::Contains(supported_types_, item->type()))
    MaybeAnimateOut();
}

std::unique_ptr<views::View> HoldingSpaceItemViewsSection::CreatePlaceholder() {
  return nullptr;
}

void HoldingSpaceItemViewsSection::DestroyPlaceholder() {
  if (!placeholder_)
    return;

  RemoveChildViewT(placeholder_);
  placeholder_ = nullptr;

  // In the absence of `placeholder_`, the `header_` should only be visible
  // when `container_` is non-empty.
  if (header_->GetVisible() && container_->children().empty())
    header_->SetVisible(false);
}

void HoldingSpaceItemViewsSection::MaybeAnimateIn() {
  if (animation_state_ & AnimationState::kAnimatingIn)
    return;

  animation_state_ |= AnimationState::kAnimatingIn;

  // NOTE: `animate_in_observer` is deleted after `OnAnimateInCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_in_observer =
      new ui::CallbackLayerAnimationObserver(base::BindRepeating(
          &HoldingSpaceItemViewsSection::OnAnimateInCompleted,
          base::Unretained(this)));

  AnimateIn(animate_in_observer);
  animate_in_observer->SetActive();
}

void HoldingSpaceItemViewsSection::MaybeAnimateOut() {
  if (animation_state_ & AnimationState::kAnimatingOut)
    return;

  animation_state_ |= AnimationState::kAnimatingOut;

  // Don't allow event processing while animating out. The views being animated
  // out may be associated with holding space items that no longer exist and
  // so should not be acted upon by the user during this time.
  SetCanProcessEventsWithinSubtree(false);

  // NOTE: `animate_out_observer` is deleted after `OnAnimateOutCompleted()`.
  ui::CallbackLayerAnimationObserver* animate_out_observer =
      new ui::CallbackLayerAnimationObserver(base::BindRepeating(
          &HoldingSpaceItemViewsSection::OnAnimateOutCompleted,
          base::Unretained(this)));

  AnimateOut(animate_out_observer);
  animate_out_observer->SetActive();
}

// TODO(dmblack): Handle grow/shrink of container.
void HoldingSpaceItemViewsSection::AnimateIn(
    ui::LayerAnimationObserver* observer) {
  const base::TimeDelta animation_duration =
      disable_animations_ ? base::TimeDelta() : kAnimationDuration;
  if (views_by_item_id_.empty() && placeholder_) {
    DoAnimateIn(placeholder_, animation_duration, observer);
    return;
  }
  DoAnimateIn(container_, animation_duration, observer);
}

// TODO(dmblack): Handle animate out of `header_` if this section is leaving.
void HoldingSpaceItemViewsSection::AnimateOut(
    ui::LayerAnimationObserver* observer) {
  const base::TimeDelta animation_duration =
      disable_animations_ ? base::TimeDelta() : kAnimationDuration;
  if (placeholder_ && placeholder_->GetVisible()) {
    DCHECK(views_by_item_id_.empty());
    DoAnimateOut(placeholder_, animation_duration, observer);
    return;
  }
  DoAnimateOut(container_, animation_duration, observer);
}

bool HoldingSpaceItemViewsSection::OnAnimateInCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingIn);
  animation_state_ &= ~AnimationState::kAnimatingIn;

  if (observer.aborted_count())
    return kDeleteObserver;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // Restore event processing that was disabled while animating out. The views
  // that have been animated in should all be associated with holding space
  // items that exist in the model.
  SetCanProcessEventsWithinSubtree(true);

  return kDeleteObserver;
}

bool HoldingSpaceItemViewsSection::OnAnimateOutCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  DCHECK(animation_state_ & AnimationState::kAnimatingOut);
  animation_state_ &= ~AnimationState::kAnimatingOut;

  if (observer.aborted_count())
    return kDeleteObserver;

  DCHECK_EQ(animation_state_, AnimationState::kNotAnimating);

  // All holding space item views are going to be removed after which views will
  // be re-added for those items which still exist. A `ScopedSelectionRestore`
  // will serve to persist the current selection during this modification.
  HoldingSpaceItemViewDelegate::ScopedSelectionRestore scoped_selection_restore(
      delegate_);

  if (!container_->children().empty()) {
    container_->RemoveAllChildViews(/*delete_children=*/true);
    views_by_item_id_.clear();
  }

  DCHECK(views_by_item_id_.empty());

  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (!model)
    return kDeleteObserver;

  for (const auto& item : model->items()) {
    if (item->IsFinalized() && base::Contains(supported_types_, item->type())) {
      DCHECK(!base::Contains(views_by_item_id_, item->id()));

      // Remove the last holding space item view if already at max capacity.
      if (max_count_ && container_->children().size() == max_count_.value()) {
        auto view = container_->RemoveChildViewT(container_->children().back());
        views_by_item_id_.erase(
            HoldingSpaceItemView::Cast(view.get())->item()->id());
      }

      // Add holding space item view to the front in order to sort by recency.
      views_by_item_id_[item->id()] =
          container_->AddChildViewAt(CreateView(item.get()), 0);
    }
  }

  MaybeAnimateIn();

  return kDeleteObserver;
}

}  // namespace ash
