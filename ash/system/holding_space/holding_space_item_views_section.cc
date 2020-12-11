// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_item_views_section.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/system/holding_space/holding_space_item_view.h"
#include "ash/system/holding_space/holding_space_item_view_delegate.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Value returned during notification of animation completion events in order to
// delete the observer which provided notification.
constexpr bool kDeleteObserver = true;

// HoldingSpaceScrollView ------------------------------------------------------

class HoldingSpaceScrollView : public views::ScrollView,
                               public views::ViewObserver {
 public:
  views::View* SetContents(std::unique_ptr<views::View> view) {
    views::View* contents = views::ScrollView::SetContents(std::move(view));
    view_observer_.Observe(contents);
    return contents;
  }

 private:
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
    container_->SetVisible(false);
  } else {
    auto* scroll = AddChildView(std::make_unique<HoldingSpaceScrollView>());
    scroll->SetBackgroundColor(base::nullopt);
    scroll->ClipHeightTo(0, INT_MAX);
    scroll->SetDrawOverflowIndicator(false);
    scroll->SetPaintToLayer();
    scroll->layer()->SetFillsBoundsOpaquely(false);
    container_ = scroll->SetContents(CreateContainer());
    container_->SetVisible(false);
  }

  // Placeholder.
  auto placeholder = CreatePlaceholder();
  if (placeholder) {
    placeholder_ = AddChildView(std::move(placeholder));
    placeholder_->SetVisible(true);
    header_->SetVisible(true);
  }

  // Views.
  HoldingSpaceModel* model = HoldingSpaceController::Get()->model();
  if (model)
    OnHoldingSpaceModelAttached(model);
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
  for (const auto& item : model->items())
    OnHoldingSpaceItemAdded(item.get());
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  model_observer_.Reset();
  if (!container_->children().empty())
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  if (!item->IsFinalized())
    return;
  if (base::Contains(supported_types_, item->type()))
    MaybeAnimateOut();
}

void HoldingSpaceItemViewsSection::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  if (base::Contains(views_by_item_id_, item->id()))
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

// TODO(dmblack): Handle animate in of `placeholder_`.
// TODO(dmblack): Handle grow/shrink of container.
void HoldingSpaceItemViewsSection::AnimateIn(
    ui::LayerAnimationObserver* observer) {
  if (views_by_item_id_.empty() && placeholder_) {
    DCHECK(!placeholder_->GetVisible());
    placeholder_->SetVisible(true);
    return;
  }
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateIn(observer);
}

// TODO(dmblack): Handle animate out of `placeholder_`.
// TODO(dmblack): Handle animate out of `header_` if this section is leaving.
void HoldingSpaceItemViewsSection::AnimateOut(
    ui::LayerAnimationObserver* observer) {
  if (placeholder_ && placeholder_->GetVisible()) {
    DCHECK(views_by_item_id_.empty());
    placeholder_->SetVisible(false);
    return;
  }
  for (auto& view_by_item_id : views_by_item_id_)
    view_by_item_id.second->AnimateOut(observer);
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
