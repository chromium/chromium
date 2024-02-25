// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"

namespace {

constexpr base::TimeDelta kBoundsChangedUpdateDelay = base::Milliseconds(250);

bool IsChildOfWidgetOrWidget(const views::Widget* widget,
                             const views::Widget* potential_ancestor) {
  const views::Widget* current = widget;
  while (current != nullptr) {
    if (current == potential_ancestor) {
      return true;
    }
    current = current->parent();
  }
  return false;
}

}  // namespace

PictureInPictureOcclusionTracker::PictureInPictureOcclusionTracker() = default;

PictureInPictureOcclusionTracker::~PictureInPictureOcclusionTracker() = default;

void PictureInPictureOcclusionTracker::OnPictureInPictureWidgetOpened(
    views::Widget* picture_in_picture_widget) {
  auto iter = observed_widget_data_.find(picture_in_picture_widget);
  if (iter == observed_widget_data_.end()) {
    // If we're not already observing this widget, then create
    // ObservedWidgetData for it and start observing the widget.
    auto result = observed_widget_data_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(picture_in_picture_widget),
        std::forward_as_tuple());
    CHECK(result.second);
    iter = result.first;

    widget_observations_.AddObservation(picture_in_picture_widget);
  }

  // Update the ObservedWidgetData to indicate that this is a picture-in-picture
  // widget.
  iter->second.is_picture_in_picture_widget = true;

  // Update all observers since there's a new picture-in-picture widget that
  // can occlude.
  UpdateAllObserverStates();
}

void PictureInPictureOcclusionTracker::AddObserver(
    PictureInPictureOcclusionTrackerObserver* observer) {
  observers_.AddObserver(observer);

  // Determine if we already have the occlusion state for this widget cached.
  bool has_cached_occluded_data = false;
  auto iter = observed_widget_data_.find(observer->occludable_widget());
  if (iter != observed_widget_data_.end()) {
    has_cached_occluded_data = iter->second.number_of_direct_observers > 0;
  }

  ObserveWidgetAndParents(observer->occludable_widget());

  // Get the associated ObservedWidgetData. This must exist since it is created
  // in `ObserveWidgetAndParents()` above.
  iter = observed_widget_data_.find(observer->occludable_widget());
  CHECK(iter != observed_widget_data_.end());

  if (has_cached_occluded_data) {
    // This isn't the first observer for this widget, so we can send the cached
    // state.
    observer->occlusion_observer()->OnOcclusionStateChanged(
        iter->second.occluded);
  } else {
    // This is the first observer for this widget, so calculate and send the
    // current occlusion state.
    UpdateObserverStateForWidget(observer->occludable_widget(),
                                 /*force_update=*/true);
  }
}

void PictureInPictureOcclusionTracker::RemoveObserver(
    PictureInPictureOcclusionTrackerObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    return;
  }

  observers_.RemoveObserver(observer);

  UnobserveWidgetAndParents(observer->occludable_widget());
}

void PictureInPictureOcclusionTracker::OnWidgetDestroying(
    views::Widget* widget) {
  auto iter = observed_widget_data_.find(widget);
  CHECK(iter != observed_widget_data_.end());

  const bool is_picture_in_picture_widget =
      iter->second.is_picture_in_picture_widget;

  // Stop observing this widget regardless of how many dependent observers it
  // has, and reduce the dependent observer count of the parent widgets
  // accordingly.
  const int number_of_dependent_observers =
      iter->second.number_of_dependent_observers;
  observed_widget_data_.erase(widget);
  widget_observations_.RemoveObservation(widget);
  for (int i = 0; i < number_of_dependent_observers; i++) {
    UnobserveWidgetAndParents(widget->parent(),
                              /*directly_unobserve_this_widget=*/false);
  }

  // Remove all observers for this widget.
  for (auto& observer : observers_) {
    if (observer.occludable_widget() == widget) {
      observers_.RemoveObserver(&observer);
    }
  }

  // If a picture-in-picture widget is destroying, then we need to update all
  // observers.
  if (is_picture_in_picture_widget) {
    UpdateAllObserverStates();
  }
}

void PictureInPictureOcclusionTracker::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visibility) {
  auto iter = observed_widget_data_.find(widget);
  CHECK(iter != observed_widget_data_.end());

  // If a picture-in-picture widget has changed visibility, then all observers
  // need to be updated.
  if (iter->second.is_picture_in_picture_widget) {
    UpdateAllObserverStates();
  }
}

void PictureInPictureOcclusionTracker::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  // Ignore extra calls while the throttle timer is running.
  if (bounds_changed_throttle_timer_.IsRunning()) {
    return;
  }

  // Immediately calculate the new occlusion states. Since bounds changes often
  // happen many times in a short period (e.g. when a user drags a window),
  // start a timer which will calculate the occlusion states again, and elide
  // all subsequent `OnWidgetBoundsChanged()` calls between now and then.
  // `base::Unretained()` is safe here since `this` owns
  // `bounds_changed_throttle_timer_`.
  UpdateAllObserverStates();
  bounds_changed_throttle_timer_.Start(
      FROM_HERE, kBoundsChangedUpdateDelay,
      base::BindOnce(&PictureInPictureOcclusionTracker::UpdateAllObserverStates,
                     base::Unretained(this)));
}

std::vector<views::Widget*>
PictureInPictureOcclusionTracker::GetPictureInPictureWidgetsForTesting() {
  std::vector<views::Widget*> pip_widgets;
  for (const auto& [widget, observed_widget_data] : observed_widget_data_) {
    if (observed_widget_data.is_picture_in_picture_widget) {
      pip_widgets.push_back(widget);
    }
  }
  return pip_widgets;
}

void PictureInPictureOcclusionTracker::SetWidgetOcclusionStateForTesting(
    views::Widget* observed_widget,
    bool occluded) {
  auto iter = observed_widget_data_.find(observed_widget);

  // Only observed widgets should have a manual occlusion state set for testing.
  CHECK(iter != observed_widget_data_.end());

  iter->second.forced_occlusion_state = occluded;

  UpdateObserverStateForWidget(observed_widget, /*force_update=*/true);
}

void PictureInPictureOcclusionTracker::ObserveWidgetAndParents(
    views::Widget* widget,
    bool directly_observe_this_widget) {
  if (!widget) {
    return;
  }

  auto iter = observed_widget_data_.find(widget);

  // If we're not already observing this widget, then create ObservedWidgetData
  // for it and start observing.
  if (iter == observed_widget_data_.end()) {
    auto result = observed_widget_data_.emplace(std::piecewise_construct,
                                                std::forward_as_tuple(widget),
                                                std::forward_as_tuple());
    CHECK(result.second);
    iter = result.first;

    widget_observations_.AddObservation(widget);
  }

  if (directly_observe_this_widget) {
    iter->second.number_of_direct_observers++;
  }

  iter->second.number_of_dependent_observers++;

  ObserveWidgetAndParents(widget->parent(),
                          /*directly_observe_this_widget=*/false);
}

void PictureInPictureOcclusionTracker::UnobserveWidgetAndParents(
    views::Widget* widget,
    bool directly_unobserve_this_widget) {
  if (!widget) {
    return;
  }

  auto iter = observed_widget_data_.find(widget);

  if (iter == observed_widget_data_.end()) {
    return;
  }

  if (directly_unobserve_this_widget) {
    CHECK_GT(iter->second.number_of_direct_observers, 0);
    iter->second.number_of_direct_observers--;
  }
  CHECK_GT(iter->second.number_of_dependent_observers, 0);
  iter->second.number_of_dependent_observers--;

  if (iter->second.number_of_dependent_observers == 0 &&
      !iter->second.is_picture_in_picture_widget) {
    // If `number_of_dependent_observers` == 0, then
    // `number_of_direct_observers` must also be zero since dependent observers
    // includes direct observers.
    CHECK_EQ(iter->second.number_of_direct_observers, 0);
    observed_widget_data_.erase(iter);
    widget_observations_.RemoveObservation(widget);
  }

  UnobserveWidgetAndParents(widget->parent(),
                            /*directly_unobserve_this_widget=*/false);
}

void PictureInPictureOcclusionTracker::UpdateAllObserverStates() {
  for (const auto& [widget, observed_widget_data] : observed_widget_data_) {
    if (observed_widget_data.number_of_direct_observers > 0) {
      UpdateObserverStateForWidget(widget);
    }
  }
}

void PictureInPictureOcclusionTracker::UpdateObserverStateForWidget(
    views::Widget* widget,
    bool force_update) {
  const gfx::Rect observer_bounds = widget->GetWindowBoundsInScreen();
  bool occluded = false;
  for (const auto& [picture_in_picture_widget, observed_widget_data] :
       observed_widget_data_) {
    // We only care about occlusions from picture-in-picture widgets.
    if (!observed_widget_data.is_picture_in_picture_widget) {
      continue;
    }

    // Invisible widgets don't occlude anything.
    if (!picture_in_picture_widget->IsVisible()) {
      continue;
    }

    // If the observer widget is a child of this picture-in-picture widget (or
    // the same widget), then it can't be occluded by it.
    if (IsChildOfWidgetOrWidget(widget, picture_in_picture_widget)) {
      continue;
    }

    const gfx::Rect picture_in_picture_bounds =
        picture_in_picture_widget->GetWindowBoundsInScreen();
    if (picture_in_picture_bounds.Intersects(observer_bounds)) {
      occluded = true;
      break;
    }
  }

  auto iter = observed_widget_data_.find(widget);
  CHECK(iter != observed_widget_data_.end());
  if (!force_update && (occluded == iter->second.occluded)) {
    return;
  }

  // Update the observers if the occlusion state has changed.
  iter->second.occluded = occluded;
  if (iter->second.forced_occlusion_state.has_value()) {
    occluded = iter->second.forced_occlusion_state.value();
  }
  for (auto& observer : observers_) {
    if (observer.occludable_widget() == widget) {
      observer.occlusion_observer()->OnOcclusionStateChanged(occluded);
    }
  }
}
