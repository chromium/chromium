// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_OBSERVER_H_

namespace resource_coordinator {

class LifecycleUnit;

// Interface to be notified when LifecycleUnits are created.
class LifecycleUnitSourceObserver {
 public:
  virtual ~LifecycleUnitSourceObserver() = default;

  // Invoked immediately after a LifecycleUnit is created.
  //
  // The observer doesn't own |lifecycle_unit|. To use |lifecycle_unit| beyond
  // this method invocation, register a LifecycleUnitObserver to be notified of
  // its destruction.
  virtual void OnLifecycleUnitCreated(LifecycleUnit* lifecycle_unit) = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_OBSERVER_H_
