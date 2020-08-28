// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"
#include "base/strings/string16.h"

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
    Result(const std::string& id, ash::SearchResultType type)
        : id(id), type(type) {}

    std::string id;
    ash::SearchResultType type = ash::SEARCH_RESULT_TYPE_BOUNDARY;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when |results| have been displayed for the length of the
    // impression timer.
    virtual void OnImpression(Location location,
                              const std::vector<Result>& results,
                              const base::string16& query) {}

    // Called when an impression occurred for |results|, and the user then moved
    // to a different UI view. For example, by closing the launcher or
    // changing the search query.
    virtual void OnAbandon(Location location,
                           const std::vector<Result>& results,
                           const base::string16& query) {}

    // Called when the |location| UI view displayed |results|, but the user
    // launched a result in a different UI view. This can only happen when
    // |location| is kList or kTile.
    virtual void OnIgnore(Location location,
                          const std::vector<Result>& results,
                          const base::string16& query) {}

    // Called when the |launched| result is launched, and provides all |shown|
    // results at |location| (including |launched|).
    virtual void OnLaunch(Location location,
                          const Result& launched,
                          const std::vector<Result>& shown,
                          const base::string16& query) {}
  };

  virtual ~AppListNotifier() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called to indicate a search |result| has been launched at the UI surface
  // |location|.
  virtual void NotifyLaunched(Location location, const Result& result) = 0;

  // Called to indicate the results displayed in the |location| UI surface have
  // changed. |results| should contain a complete list of what is now shown.
  virtual void NotifyResultsUpdated(Location location,
                                    const std::vector<Result>& results) = 0;

  // Called to indicate the user has updated the search query to |query|.
  virtual void NotifySearchQueryChanged(const base::string16& query) = 0;

  // Called to indicate the UI state is now |view|.
  virtual void NotifyUIStateChanged(AppListViewState view) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_NOTIFIER_H_
