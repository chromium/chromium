// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_OBSERVER_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "content/public/browser/visibility.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitState;
using ::mojom::LifecycleUnitStateChangeReason;

class LifecycleUnit;

// Interface to be notified when the state of a LifecycleUnit changes.
class LifecycleUnitObserver {
 public:
  virtual ~LifecycleUnitObserver();

  // Invoked when the state of the observed LifecycleUnit changes.
  virtual void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason);

  // Invoked when the visibility of the observed LifecyleUnit changes.
  virtual void OnLifecycleUnitVisibilityChanged(LifecycleUnit* lifecycle_unit,
                                                content::Visibility visibility);

  // Invoked before the observed LifecycleUnit starts being destroyed (i.e.
  // |lifecycle_unit| is still valid when this is invoked).
  virtual void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_OBSERVER_H_
