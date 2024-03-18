// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A notifier interface implemented in Chrome and called from Ash, which allows
// objects in Chrome to observe state changes in Ash. Its main use is to signal
// events related to metrics and logging: search result impressions, abandons,
// ignores, and launches. See method comments for definitions of these.
//
// Implementing notifiers guarantee that calls to each observer come in pairs.
// Every OnImpression call will be followed by a call to one of OnAbandon,
// OnIgnore, or OnLaunch, and vice versa. Each pair of calls will be passed
// the same results vector.
class ASH_PUBLIC_EXPORT AppListNotifier {
 public:
  using Location = ash::SearchResultDisplayType;

  struct Result {
    Result(const std::string& id,
           ash::SearchResultType type,
           const std::optional<ash::ContinueFileSuggestionType>&
               continue_file_type)
        : id(id), type(type), continue_file_type(continue_file_type) {}

    std::string id;
    ash::SearchResultType type = ash::SEARCH_RESULT_TYPE_BOUNDARY;
    std::optional<ash::ContinueFileSuggestionType> continue_file_type;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the search query is first updated after activating the search
    // box or the app list view state transitions to kFullscreenSearch.
    virtual void OnSearchSessionStarted() {}

    // Called when an active search session ends when exiting bubble launcher
    // search or the app list view state transitions out of kFullscreenSearch.
    virtual void OnSearchSessionEnded(const std::u16string& query) {}

    // Called when |results| have been displayed for the length of the
    // impression timer.
    virtual void OnSeen(Location location,
                        const std::vector<Result>& results,
                        const std::u16string& query) {}

    // Called when |results| have been displayed for the length of the
    // impression timer, launched, or ignored.
    virtual void OnImpression(Location location,
                              const std::vector<Result>& results,
                              const std::u16string& query) {}

    // Called when an impression occurred for |results|, and the user then moved
    // to a different UI view. For example, by closing the launcher or
    // changing the search query.
    virtual void OnAbandon(Location location,
                           const std::vector<Result>& results,
                           const std::u16string& query) {}

    // Called when the |location| UI view displayed |results|, but the user
    // launched a result in a different UI view. This can only happen when
    // |location| is kContinue or kRecentApps.
    virtual void OnIgnore(Location location,
                          const std::vector<Result>& results,
                          const std::u16string& query) {}

    // Called when the |launched| result is launched, and provides all |shown|
    // results at |location| (including |launched|).
    virtual void OnLaunch(Location location,
                          const Result& launched,
                          const std::vector<Result>& shown,
                          const std::u16string& query) {}

    // Called immediately when the search |query| changes.
    virtual void OnQueryChanged(const std::u16string& query) {}
  };

  virtual ~AppListNotifier() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called when visibility of a container within the launcher continue section
  // (continue task suggestions, or recent apps) changes.
  virtual void NotifyContinueSectionVisibilityChanged(Location location,
                                                      bool visible) = 0;

  // Called to indicate a search |result| has been launched at the UI surface
  // |location|.
  virtual void NotifyLaunched(Location location, const Result& result) = 0;

  // Called to indicate the results displayed in the |location| UI surface have
  // changed. |results| should contain a complete list of what is now shown.
  virtual void NotifyResultsUpdated(Location location,
                                    const std::vector<Result>& results) = 0;

  // Called to indicate the user has updated the search query to |query|.
  virtual void NotifySearchQueryChanged(const std::u16string& query) = 0;

  // Fires a timer to report search result impression for the provided
  // location, if an impression update was scheduled. Returns whether a timer
  // was fired.
  virtual bool FireImpressionTimerForTesting(Location location) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
