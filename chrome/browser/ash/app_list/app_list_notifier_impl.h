// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace base {
class OneShotTimer;
}

namespace ash {
class AppListController;
}

// This AppListNotifier subclass is only used for the productivity launcher.
//
// Chrome implementation of the AppListNotifier. This is mainly responsible for
// translating notifications about launcher UI events - eg. launcher opened,
// results changed, etc. - into impression, launch, and abandon notifications
// for observers. See the header comment in app_list_notifier.h for definitions
// of these.
//
// This handles results on each search result view, which are are just called
// _views_ in this comment.
//
// State machine
// =============
//
// This class implements N state machines, one for each view, that are mostly
// independent. Each state machine can be in one of three primary states:
//
//  - kNone: the view is not displayed.
//  - kShown: the view is displayed.
//  - kSeen: the same results on the view have been displayed for a certain
//           amount of time.
//
// There are two extra 'transitional' states:
//
//  - kLaunched: a user has launched a result in this surface.
//  - kIgnored: a user has launched a result in another visible surface.
//
// These states exist only to simplify implementation, and the state machine
// doesn't stay in them for any length of time. Immediately after a transition
// to kLaunch or kIgnore, the launcher closes and the state is set to kNone.
//
// Various user and background events cause _transitions_ between states. The
// notifier performs _actions_ on a transition based on the (from, to) pair of
// states.
//
// Each state machine is associated with an _impression timer_ that begins on a
// transition to kShown. Once the timer finishes, it causes a transition to
// kSeen.
//
// Transitions
// ===========
//
// The events that cause a transition to a state are as follows.
//
//  - kNone: closing the launcher or moving to a different view.
//  - kShown: opening the relevant view, or changing the search query if the
//            view is the app tiles or results list.
//  - kLaunched: launching a result.
//  - kSeen: the impression timer finishing for the view.
//
// Actions
// =======
//
// These actions are triggered on a state transition.
//
//  From -> To         | Actions
//  -------------------|--------------------------------------------------------
//  kNone -> kNone     | No action.
//                     |
//  kNone -> kShown    | Start impression timer, as view just displayed.
//                     |
//  kShown -> kNone    | Cancel impression timer, as view changed.
//                     |
//  kShown -> kSeen    | Notify of an impression, as impression timer finished.
//                     |
//  kShown -> kShown   | Restart impression timer. Should only be triggered for
//                     | the list view, when the displayed results change.
//                     |
//                     |
//  kSeen -> kLaunch   | Notify of a launch and immediately set state to kNone,
//                     | as user launched a result.
//                     |
//  kShown -> kLaunch  | Notify of a launch and impression then set state to
//                     | kNone and cancel impression timer, as user launched
//                     | result so must have seen the results.
//                     |
//                     |
//  kShown -> kIgnored | Notify of an ignore and impression, then set state to
//                     | kNone and cancel timer, as user launched a result in
//                     | a different view to must have seen the results.
//                     |
//  kSeen -> kIgnored  | Notify of an ignore, then set state to kNone, as user
//                     | launched a result in a different view.
//                     |
//                     |
//  kSeen -> kNone     | Notify of an abandon, as user closed the launcher.
//                     |
//  kSeen -> kShown    | Notify of an abandon and restart timer, as user saw
//                     | results but changed view or updated the search query.
//
// The transitions we consider impossible are kNone -> {kSeen, kLaunch},
// kSeen -> kSeen, and {kLaunch, kIgnored } -> anything, because kLaunch and
// kIgnored are temporary states.
//
// Discussion
// ==========
//
// Warning: NotifyResultsUpdated cannot be used as a signal of user actions or
// UI state. Results can be updated at any time for any UI view, regardless
// of the state of the launcher or what the user is doing.
class AppListNotifierImpl : public ash::AppListNotifier,
                            public ash::AppListControllerObserver {
 public:
  explicit AppListNotifierImpl(ash::AppListController* app_list_controller);
  ~AppListNotifierImpl() override;

  AppListNotifierImpl(const AppListNotifierImpl&) = delete;
  AppListNotifierImpl& operator=(const AppListNotifierImpl&) = delete;

  // AppListNotifier:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyLaunched(Location location, const Result& result) override;
  void NotifyResultsUpdated(Location location,
                            const std::vector<Result>& results) override;
  void NotifyContinueSectionVisibilityChanged(Location location,
                                              bool visible) override;
  void NotifySearchQueryChanged(const std::u16string& query) override;
  bool FireImpressionTimerForTesting(Location location) override;

  // AppListControllerObserver:
  void OnAppListVisibilityWillChange(bool shown, int64_t display_id) override;
  void OnViewStateChanged(ash::AppListViewState state) override;

 private:
  // Possible states of the state machine.
  enum class State {
    kNone,
    kShown,
    kSeen,
    kLaunched,
    kIgnored,
  };

  // Performs all state transition logic.
  void DoStateTransition(Location location, State new_state);

  // (Re)starts the impression timer for |location|.
  void RestartTimer(Location location);

  // Stops the impression timer for |location|.
  void StopTimer(Location location);

  // Handles a finished impression timer for |location|.
  void OnTimerFinished(Location location);

  // Returns the stored results for |location|.
  std::vector<Result> ResultsForLocation(Location location);

  // Returns whether a continue section container (or recent apps container) are
  // reported to be visible.
  bool GetContinueSectionVisibility(Location location) const;

  const raw_ptr<ash::AppListController> app_list_controller_;

  base::ObserverList<Observer> observers_;

  // The current state of each state machine.
  base::flat_map<Location, State> states_;

  // The reported visibility state of app list continue section - used for
  // `Location::kContinue` and `Location::kRecentApps`, which may remain hidden
  // while app list is visible.
  base::flat_map<Location, bool> continue_section_visibility_;

  // An impression timer for each state machine.
  base::flat_map<Location, std::unique_ptr<base::OneShotTimer>> timers_;

  // Whether or not the app list is shown.
  bool shown_ = false;
  // Whether or not a search session is in progress.
  bool search_session_in_progress_ = false;
  // The currently shown results for each UI view.
  base::flat_map<Location, std::vector<Result>> results_;
  // The current search query, may be empty.
  std::u16string query_;
  // The most recently launched result.
  std::optional<Result> launched_result_;

  // Special-case for the results at Location::kList. These need to be
  // accumulated until the query changes, rather than set like other result
  // types. The keys are result IDs, and the values are wrapped in an optional
  // because Result is not default-constructable.
  //
  // TODO(crbug.com/40184658): This can be removed once SearchResultListView has
  // its notifier calls updated.
  base::flat_map<std::string, std::optional<Result>> list_results_;

  base::WeakPtrFactory<AppListNotifierImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_NOTIFIER_IMPL_H_
