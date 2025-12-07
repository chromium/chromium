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
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "content/public/browser/visibility.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitDiscardReason;
using ::mojom::LifecycleUnitLoadingState;
using ::mojom::LifecycleUnitState;

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

  // Returns the last time ticks at which the LifecycleUnit was focused, or
  // base::TimeTicks::Max() if the LifecycleUnit is currently focused.
  virtual base::TimeTicks GetLastFocusedTimeTicks() const = 0;

  // Returns the last time at which the LifecycleUnit was focused, or
  // base::Time::Max() if the LifecycleUnit is currently focused.
  virtual base::Time GetLastFocusedTime() const = 0;

  // Returns the loading state associated with a LifecycleUnit.
  virtual LifecycleUnitLoadingState GetLoadingState() const = 0;

  // Returns the current state of this LifecycleUnit.
  virtual LifecycleUnitState GetState() const = 0;

  // Returns the last time at which the state of this LifecycleUnit changed.
  virtual base::TimeTicks GetStateChangeTime() const = 0;

  // Request that the LifecycleUnit be loaded, return true if the request is
  // successful.
  virtual bool Load() = 0;

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
};

using LifecycleUnitSet =
    base::flat_set<raw_ptr<LifecycleUnit, CtnExperimental>>;
using LifecycleUnitVector =
    std::vector<raw_ptr<LifecycleUnit, VectorExperimental>>;

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
