// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_container_view.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"

namespace {

constexpr base::TimeDelta kFadeInDuration = base::Milliseconds(100);
constexpr base::TimeDelta kIdentityTranslationDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kFastFadeInDuration = base::Milliseconds(0);

// Show animations for search result views and titles have a translation
// distance of 'kAnimatedOffsetMultiplier' * i where i is the position of the
// view in the 'AppListSearchView'.
constexpr int kAnimatedOffsetMultiplier = 4;

}  // namespace

namespace ash {

SearchResultContainerView::SearchResultContainerView(
    AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate);
}

SearchResultContainerView::~SearchResultContainerView() {
  if (results_ && active_)
    results_->RemoveObserver(this);
}

void SearchResultContainerView::SetResults(
    SearchModel::SearchResults* results) {
  if (results_ && active_)
    results_->RemoveObserver(this);

  results_ = results;
  if (results_ && active_)
    results_->AddObserver(this);

  if (active_)
    Update();
}

void SearchResultContainerView::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;

  if (!active_)
    update_factory_.InvalidateWeakPtrs();

  if (results_) {
    if (active_)
      results_->AddObserver(this);
    else
      results_->RemoveObserver(this);
  }
}

void SearchResultContainerView::ResetAndHide() {
  SetVisible(false);

  for (size_t i = 0; i < num_results(); ++i) {
    SearchResultBaseView* const result_view = GetResultViewAt(i);
    result_view->SetResult(nullptr);
    result_view->SetVisible(false);
  }
  num_results_ = 0;
}

std::optional<SearchResultContainerView::ResultsAnimationInfo>
SearchResultContainerView::ScheduleResultAnimations(
    const ResultsAnimationInfo& aggregate_animation_info) {
  // Collect current container animation info.
  ResultsAnimationInfo current_animation_info;

  if (num_results() < 1 || !enabled_) {
    UpdateResultsVisibility(/*force_hide=*/true);
    return current_animation_info;
  }

  // All views should be animated if
  // *   the container is being shown, or
  // *   any of the result views that precede the container in the search UI are
  //     animating, or
  // *   if the first animating result view is in a preceding container.
  bool force_animation =
      !GetVisible() || aggregate_animation_info.animating_views > 0 ||
      aggregate_animation_info.first_animated_result_view_index <=
          aggregate_animation_info.total_result_views;

  UpdateResultsVisibility(/*force_hide=*/false);
  current_animation_info.use_short_animations =
      aggregate_animation_info.use_short_animations;

  auto schedule_animation =
      [&current_animation_info,
       &aggregate_animation_info](views::View* animated_view) {
        ShowViewWithAnimation(animated_view,
                              current_animation_info.total_views +
                                  aggregate_animation_info.total_views,
                              current_animation_info.use_short_animations);
        ++current_animation_info.animating_views;
      };

  views::View* title_label = GetTitleLabel();
  if (title_label && title_label->GetVisible()) {
    if (force_animation) {
      schedule_animation(title_label);
    }
    ++current_animation_info.total_views;
  }

  for (auto* result_view : GetViewsToAnimate()) {
    // Checks whether the index of the current result view is greater than
    // or equal to the index of the first result view that should be animated.
    // Force animations if true.
    if (aggregate_animation_info.total_result_views +
            current_animation_info.total_result_views >=
        aggregate_animation_info.first_animated_result_view_index) {
      force_animation = true;
    }
    if (force_animation) {
      schedule_animation(result_view);
    }

    ++current_animation_info.total_views;
    ++current_animation_info.total_result_views;
  }

  return current_animation_info;
}

void SearchResultContainerView::AppendShownResultMetadata(
    std::vector<SearchResultAimationMetadata>* result_metadata_) {}

bool SearchResultContainerView::HasAnimatingChildView() {
  auto is_animating = [](views::View* view) {
    return (view->GetVisible() && view->layer() &&
            view->layer()->GetAnimator() &&
            view->layer()->GetAnimator()->is_animating());
  };

  if (is_animating(GetTitleLabel())) {
    return true;
  }

  for (auto* result_view : GetViewsToAnimate()) {
    if (is_animating(result_view)) {
      return true;
    }
  }

  return false;
}

void SearchResultContainerView::OnSelectedResultChanged() {}

void SearchResultContainerView::Update() {
  DCHECK(active_);

  update_factory_.InvalidateWeakPtrs();
  num_results_ = DoUpdate();
  DeprecatedLayoutImmediately();
  if (delegate_)
    delegate_->OnSearchResultContainerResultsChanged();
}

bool SearchResultContainerView::UpdateScheduled() {
  return update_factory_.HasWeakPtrs();
}

void SearchResultContainerView::AddObservedResultView(
    SearchResultBaseView* result_view) {
  result_view_observations_.AddObservation(result_view);
}

void SearchResultContainerView::RemoveObservedResultView(
    SearchResultBaseView* result_view) {
  result_view_observations_.RemoveObservation(result_view);
}

void SearchResultContainerView::ListItemsAdded(size_t /*start*/,
                                               size_t /*count*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemsRemoved(size_t /*start*/,
                                                 size_t /*count*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemMoved(size_t /*index*/,
                                              size_t /*target_index*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemsChanged(size_t /*start*/,
                                                 size_t /*count*/) {
  ScheduleUpdate();
}

SearchResultBaseView* SearchResultContainerView::GetFirstResultView() {
  return num_results_ <= 0 ? nullptr : GetResultViewAt(0);
}

bool SearchResultContainerView::RunScheduledUpdateForTest() {
  if (!UpdateScheduled())
    return false;
  Update();
  return true;
}

// static
void SearchResultContainerView::ShowViewWithAnimation(
    views::View* result_view,
    int position,
    bool use_short_animations) {
  DCHECK(result_view->layer()->GetAnimator());

  // Abort any in-progress layer animation.
  result_view->layer()->GetAnimator()->AbortAllAnimations();

  // Animation spec:
  //
  // Y Position: Down (offset) → End position
  // offset: position * kAnimatedOffsetMultiplier px
  // Duration: 200ms
  // Ease: (0.00, 0.00, 0.20, 1.00)

  // Opacity: 0% -> 100%
  // Duration: 100 ms
  // Ease: Linear

  gfx::Transform translate_down;
  translate_down.Translate(0, position * kAnimatedOffsetMultiplier);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(result_view, 0.0f)
      .SetTransform(result_view, translate_down)
      .Then()
      .SetOpacity(result_view, 1.0f, gfx::Tween::LINEAR)
      .SetDuration(use_short_animations ? kFastFadeInDuration : kFadeInDuration)
      .At(base::TimeDelta())
      .SetDuration(
          use_short_animations
              ? app_list_features::DynamicSearchUpdateAnimationDuration()
              : kIdentityTranslationDuration)
      .SetTransform(result_view, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN);
}

void SearchResultContainerView::ScheduleUpdate() {
  if (!active_)
    return;

  // When search results are added one by one, each addition generates an update
  // request. Consolidates those update requests into one Update call.
  if (!update_factory_.HasWeakPtrs()) {
    if (delegate_)
      delegate_->OnSearchResultContainerResultsChanging();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SearchResultContainerView::Update,
                                  update_factory_.GetWeakPtr()));
  }
}

BEGIN_METADATA(SearchResultContainerView)
END_METADATA

}  // namespace ash
