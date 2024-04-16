// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_list_notifier_impl.h"

#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/check.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/display/types/display_constants.h"

namespace {

// TODO(crbug.com/40128586): Finalize a value for this, and possibly use
// different values for different UI surfaces.
constexpr base::TimeDelta kImpressionTimer = base::Seconds(1);

}  // namespace

AppListNotifierImpl::AppListNotifierImpl(
    ash::AppListController* app_list_controller)
    : app_list_controller_(app_list_controller) {
  DCHECK(app_list_controller_);
  app_list_controller_->AddObserver(this);
  OnAppListVisibilityWillChange(app_list_controller_->IsVisible(),
                                display::kInvalidDisplayId);
}

AppListNotifierImpl::~AppListNotifierImpl() {
  app_list_controller_->RemoveObserver(this);
}

void AppListNotifierImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppListNotifierImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppListNotifierImpl::NotifyLaunched(Location location,
                                         const Result& result) {
  launched_result_ = result;

  // The continue and recent apps appear at the same time. If a launch occurs in
  // one, mark the other as 'ignored' rather than abandoned.
  if (location == Location::kContinue) {
    DoStateTransition(Location::kRecentApps, State::kIgnored);
  } else if (location == Location::kRecentApps) {
    DoStateTransition(Location::kContinue, State::kIgnored);
  }

  DoStateTransition(location, State::kLaunched);
}

void AppListNotifierImpl::NotifyResultsUpdated(
    Location location,
    const std::vector<Result>& results) {
  if (location == Location::kList) {
    for (const auto& result : results)
      list_results_[result.id] = result;
  } else if (location == Location::kAnswerCard ||
             location == Location::kImage) {
    if (results.size() > 0) {
      DoStateTransition(location, State::kShown);
    } else {
      DoStateTransition(location, State::kNone);
    }
    results_[location] = results;
  } else {
    results_[location] = results;
  }
}

void AppListNotifierImpl::NotifyContinueSectionVisibilityChanged(
    Location location,
    bool visible) {
  DCHECK(location == Location::kContinue || location == Location::kRecentApps);

  continue_section_visibility_[location] = visible;
  DoStateTransition(location, shown_ && query_.empty() && visible
                                  ? State::kShown
                                  : State::kNone);
}

bool AppListNotifierImpl::GetContinueSectionVisibility(
    Location location) const {
  const auto it = continue_section_visibility_.find(location);
  return it != continue_section_visibility_.cend() && it->second;
}

void AppListNotifierImpl::NotifySearchQueryChanged(
    const std::u16string& query) {
  // In some cases the query can change after the launcher is closed, in
  // particular this happens when abandoning the launcher with a non-empty
  // query. Only do a state transition if the launcher is open.
  if (shown_) {
    if (query.empty()) {
      DoStateTransition(Location::kList, State::kNone);
      DoStateTransition(Location::kAnswerCard, State::kNone);
      DoStateTransition(Location::kContinue,
                        GetContinueSectionVisibility(Location::kContinue)
                            ? State::kShown
                            : State::kNone);
      DoStateTransition(Location::kRecentApps,
                        GetContinueSectionVisibility(Location::kRecentApps)
                            ? State::kShown
                            : State::kNone);
    } else {
      if (!search_session_in_progress_) {
        search_session_in_progress_ = true;
        for (auto& observer : observers_) {
          observer.OnSearchSessionStarted();
        }
      }

      DoStateTransition(Location::kList, State::kShown);
      DoStateTransition(Location::kAnswerCard, State::kNone);
      DoStateTransition(Location::kContinue, State::kNone);
      DoStateTransition(Location::kRecentApps, State::kNone);
    }
  }

  // Update the stored |query_| after performing the state transitions, so that
  // an abandon triggered by the query change correctly uses the pre-abandon
  // query.
  query_ = query;

  results_.clear();
  list_results_.clear();

  for (auto& observer : observers_) {
    observer.OnQueryChanged(query);
  }
}

bool AppListNotifierImpl::FireImpressionTimerForTesting(Location location) {
  auto timer_it = timers_.find(location);
  if (timer_it == timers_.end() || !timer_it->second->IsRunning())
    return false;
  timer_it->second->FireNow();
  return true;
}

void AppListNotifierImpl::OnAppListVisibilityWillChange(bool shown,
                                                        int64_t display_id) {
  if (shown_ == shown)
    return;
  shown_ = shown;

  if (shown) {
    if (GetContinueSectionVisibility(Location::kContinue))
      DoStateTransition(Location::kContinue, State::kShown);
    if (GetContinueSectionVisibility(Location::kRecentApps))
      DoStateTransition(Location::kRecentApps, State::kShown);
    // kList is not shown until a search query is entered.
  } else {
    if (search_session_in_progress_) {
      search_session_in_progress_ = false;
      for (auto& observer : observers_) {
        observer.OnSearchSessionEnded(query_);
      }
    }

    DoStateTransition(Location::kList, State::kNone);
    DoStateTransition(Location::kContinue, State::kNone);
    DoStateTransition(Location::kRecentApps, State::kNone);
    DoStateTransition(Location::kAnswerCard, State::kNone);
  }
}

void AppListNotifierImpl::OnViewStateChanged(ash::AppListViewState state) {
  if (state == ash::AppListViewState::kFullscreenSearch && !query_.empty()) {
    search_session_in_progress_ = true;
    for (auto& observer : observers_) {
      observer.OnSearchSessionStarted();
    }
  } else {
    search_session_in_progress_ = false;
    for (auto& observer : observers_) {
      observer.OnSearchSessionEnded(query_);
    }
  }
}

void AppListNotifierImpl::RestartTimer(Location location) {
  if (timers_.find(location) == timers_.end()) {
    timers_[location] = std::make_unique<base::OneShotTimer>();
  }

  auto& timer = timers_[location];
  if (timer->IsRunning()) {
    timer->Stop();
  }
  // base::Unretained is safe here because the timer is a member of |this|, and
  // OneShotTimer cancels its timer on destruction.
  timer->Start(FROM_HERE, kImpressionTimer,
               base::BindOnce(&AppListNotifierImpl::OnTimerFinished,
                              base::Unretained(this), location));
}

void AppListNotifierImpl::StopTimer(Location location) {
  const auto it = timers_.find(location);
  if (it != timers_.end() && it->second->IsRunning()) {
    it->second->Stop();
  }
}

void AppListNotifierImpl::OnTimerFinished(Location location) {
  DoStateTransition(location, State::kSeen);
}

std::vector<AppListNotifierImpl::Result>
AppListNotifierImpl::ResultsForLocation(Location location) {
  // Special case kList, see header comment on |list_results_|.
  if (location == Location::kList) {
    std::vector<Result> results;
    for (const auto& id_result : list_results_) {
      results.push_back(id_result.second.value());
    }
    return results;
  }

  return results_[location];
}

void AppListNotifierImpl::DoStateTransition(Location location,
                                            State new_state) {
  const State old_state = states_[location];

  // Update most recent state. We special-case kLaunched and kIgnored, which are
  // temporary states reflecting a launch either in |location| or another view.
  // They immediately transition to kNone because the launcher closes after a
  // launch.
  if (new_state == State::kLaunched || new_state == State::kIgnored) {
    states_[location] = State::kNone;
  } else {
    states_[location] = new_state;
  }

  // These overlapping cases are equivalent to the explicit cases in the header
  // comment.

  // Restart timer on * -> kShown
  if (new_state == State::kShown) {
    RestartTimer(location);
  }

  // Stop timer on kShown -> {kNone, kLaunch}.
  if (old_state == State::kShown &&
      (new_state == State::kNone || new_state == State::kLaunched)) {
    StopTimer(location);
  }

  // Notify of seen on kShown -> {kSeen} when there is one or more result for
  // `location`.
  if (old_state == State::kShown && new_state == State::kSeen) {
    auto results = ResultsForLocation(location);
    if (results.size() > 0) {
      for (auto& observer : observers_) {
        observer.OnSeen(location, results, query_);
      }
    }
  }

  // Notify of impression on kShown -> {kSeen, kIgnored, kLaunched}.
  if (old_state == State::kShown &&
      (new_state == State::kSeen || new_state == State::kLaunched ||
       new_state == State::kIgnored)) {
    for (auto& observer : observers_) {
      observer.OnImpression(location, ResultsForLocation(location), query_);
    }
  }

  // Notify of launch on * -> kLaunched.
  if (new_state == State::kLaunched && launched_result_.has_value()) {
    for (auto& observer : observers_) {
      observer.OnLaunch(location, launched_result_.value(),
                        ResultsForLocation(location), query_);
    }
  }

  // Notify of ignore on * -> kIgnored.
  if (new_state == State::kIgnored) {
    for (auto& observer : observers_) {
      observer.OnIgnore(location, ResultsForLocation(location), query_);
    }
  }

  // Notify of abandon on kSeen -> {kNone, kShown}.
  if (old_state == State::kSeen &&
      (new_state == State::kNone || new_state == State::kShown)) {
    for (auto& observer : observers_) {
      observer.OnAbandon(location, ResultsForLocation(location), query_);
    }
  }
}
