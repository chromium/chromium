// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_H_

namespace resource_coordinator {

class LifecycleUnitSourceObserver;

// Interface for a class that creates and destroys LifecycleUnits.
class LifecycleUnitSource {
 public:
  virtual ~LifecycleUnitSource() = default;

  // Adds / removes an observer that is notified when a LifecycleUnit is created
  // or destroyed by this LifecycleUnitSource.
  virtual void AddObserver(LifecycleUnitSourceObserver* observer) = 0;
  virtual void RemoveObserver(LifecycleUnitSourceObserver* observer) = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_H_
