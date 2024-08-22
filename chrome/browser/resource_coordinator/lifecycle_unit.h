// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/decision_details.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "content/public/browser/visibility.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitDiscardReason;
using ::mojom::LifecycleUnitLoadingState;
using ::mojom::LifecycleUnitState;

class DecisionDetails;
class LifecycleUnitObserver;
class LifecycleUnitSource;
class TabLifecycleUnitExternal;

// A LifecycleUnit represents a unit that can switch between the "loaded" and
// "discarded" states. When it is loaded, the unit uses system resources and
// provides functionality to the user. When it is discarded, the unit doesn't
// use any system resource.
class LifecycleUnit {
 public:
  // Used to sort LifecycleUnit by importance using the last focused time.
  // The most important LifecycleUnit has the greatest SortKey.
  struct SortKey {

    SortKey();

    // Creates a SortKey based on the LifecycleUnit's last focused time.
    explicit SortKey(base::TimeTicks last_focused_time);

    SortKey(const SortKey& other);
    SortKey& operator=(const SortKey& other);

    bool operator<(const SortKey& other) const;
    bool operator>(const SortKey& other) const;

    // Last time at which the LifecycleUnit was focused. base::TimeTicks::Max()
    // if the LifecycleUnit is currently focused.
    base::TimeTicks last_focused_time;
  };

  virtual ~LifecycleUnit();

  // Returns the LifecycleUnitSource associated with this unit.
  virtual LifecycleUnitSource* GetSource() const = 0;

  // Returns the TabLifecycleUnitExternal associated with this LifecycleUnit, if
  // any.
  virtual TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() = 0;

  // Returns a unique id representing this LifecycleUnit.
  virtual int32_t GetID() const = 0;

  // Returns a title describing this LifecycleUnit, or an empty string if no
  // title is available.
  virtual std::u16string GetTitle() const = 0;

  // Returns the last time ticks at which the LifecycleUnit was focused, or
  // base::TimeTicks::Max() if the LifecycleUnit is currently focused.
  virtual base::TimeTicks GetLastFocusedTimeTicks() const = 0;

  // Returns the last time at which the LifecycleUnit was focused, or
  // base::Time::Max() if the LifecycleUnit is currently focused.
  virtual base::Time GetLastFocusedTime() const = 0;

  // Returns the current visibility of this LifecycleUnit.
  virtual content::Visibility GetVisibility() const = 0;

  // Returns the TimeTicks from when the LifecycleUnit was hidden, or
  // TimeTicks::Max() if it is currently visible.
  virtual base::TimeTicks GetWallTimeWhenHidden() const = 0;

  // Returns the Chrome usage time from when the LifecycleUnit was hidden, or
  // TimeDelta::Max() if it is currently visible.
  virtual base::TimeDelta GetChromeUsageTimeWhenHidden() const = 0;

  // Returns the loading state associated with a LifecycleUnit.
  virtual LifecycleUnitLoadingState GetLoadingState() const = 0;

  // Returns the process hosting this LifecycleUnit. Used to distribute OOM
  // scores.
  //
  // TODO(fdoray): Change this to take into account the fact that a
  // LifecycleUnit can be hosted in multiple processes. https://crbug.com/775644
  virtual base::ProcessHandle GetProcessHandle() const = 0;

  // Returns a key that can be used to evaluate the relative importance of this
  // LifecycleUnit. This key may not be trivial to calculate, so this should not
  // be called repeatedly if the value will be reused, e.g. during a sort.
  //
  // TODO(fdoray): Figure out if GetSortKey() and CanDiscard() should be
  // replaced with a method that returns a numeric value representing the
  // expected user pain caused by a discard. A values above a given threshold
  // would be equivalent to CanDiscard() returning false for a given
  // mojom::LifecycleUnitDiscardReason. https://crbug.com/775644
  virtual SortKey GetSortKey() const = 0;

  // Returns the current state of this LifecycleUnit.
  virtual LifecycleUnitState GetState() const = 0;

  // Returns the last time at which the state of this LifecycleUnit changed.
  virtual base::TimeTicks GetStateChangeTime() const = 0;

  // Request that the LifecycleUnit be loaded, return true if the request is
  // successful.
  virtual bool Load() = 0;

  // Returns the estimated number of kilobytes that would be freed if this
  // LifecycleUnit was discarded.
  //
  // TODO(fdoray): Consider exposing this only on a new class that represents a
  // group of LifecycleUnits. It is easier to compute memory consumption
  // accurately for a group of LifecycleUnits that live in the same process(es)
  // than for individual LifecycleUnits. https://crbug.com/775644
  virtual int GetEstimatedMemoryFreedOnDiscardKB() const = 0;

  // Returns true if this LifecycleUnit can be discarded. Full details regarding
  // the policy decision are recorded in the |decision_details|, for logging.
  // Returning false but with an empty |decision_details| means the transition
  // is not possible for a trivial reason that doesn't need to be reported
  // (ie, the page is already discarded).
  virtual bool CanDiscard(LifecycleUnitDiscardReason reason,
                          DecisionDetails* decision_details) const = 0;

  // Discards this LifecycleUnit.
  //
  // TODO(fdoray): Consider handling urgent discard with groups of
  // LifecycleUnits. On urgent discard, we want to minimize memory accesses. It
  // is easier to achieve that if we discard a group of LifecycleUnits that live
  // in the same process(es) than if we discard individual LifecycleUnits.
  // https://crbug.com/775644
  virtual bool Discard(LifecycleUnitDiscardReason discard_reason,
                       uint64_t resident_set_size_estimate = 0) = 0;

  // Returns the number of times this lifecycle unit has been discarded.
  virtual size_t GetDiscardCount() const = 0;

  // Returns the most recent discard reason that was applied to this lifecycle
  // unit. This only makes sense if the lifecycle unit has ever been discarded.
  virtual LifecycleUnitDiscardReason GetDiscardReason() const = 0;

  // Adds/removes an observer to this LifecycleUnit.
  virtual void AddObserver(LifecycleUnitObserver* observer) = 0;
  virtual void RemoveObserver(LifecycleUnitObserver* observer) = 0;

  // Returns the UKM source ID associated with this LifecycleUnit, if it has
  // one.
  virtual ukm::SourceId GetUkmSourceId() const = 0;
};

using LifecycleUnitSet =
    base::flat_set<raw_ptr<LifecycleUnit, CtnExperimental>>;
using LifecycleUnitVector =
    std::vector<raw_ptr<LifecycleUnit, VectorExperimental>>;

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
