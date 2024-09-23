// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_BASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_BASE_H_

#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source.h"

namespace resource_coordinator {

class LifecycleUnitBase;

// Base class for a class that creates and destroys LifecycleUnits.
class LifecycleUnitSourceBase : public LifecycleUnitSource {
 public:
  LifecycleUnitSourceBase();

  LifecycleUnitSourceBase(const LifecycleUnitSourceBase&) = delete;
  LifecycleUnitSourceBase& operator=(const LifecycleUnitSourceBase&) = delete;

  ~LifecycleUnitSourceBase() override;

  // LifecycleUnitSource:
  void AddObserver(LifecycleUnitSourceObserver* observer) override;
  void RemoveObserver(LifecycleUnitSourceObserver* observer) override;

  size_t lifecycle_unit_count() const { return lifecycle_unit_count_; }

 protected:
  friend class LifecycleUnitBase;

  // Called by LifecycleUnitBase when a new one is created. Used to update
  // |lifecycle_unit_count|. Note that the |lifecycle_unit| will be half built
  // at this point so not usable.
  void NotifyLifecycleUnitBeingCreated(LifecycleUnitBase* lifecycle_unit);

  // Intended to be called by the a concrete LifecycleUnitSourceBase
  // implementation. This dispatches the creation event to registered
  // observers.
  void NotifyLifecycleUnitCreated(LifecycleUnitBase* lifecycle_unit);

  // Called by LifecycleUnitBase when it is being torn down. Used to update
  // |lifecycle_unit_count|. Note that the |lifecycle_unit| will be half torn
  // down at this point so not usable.
  void NotifyLifecycleUnitBeingDestroyed(LifecycleUnitBase* lifecycle_unit);

  // Called when a first lifecycle unit comes into being. This is an empty stub
  // which can optionally be overridden by derived classes.
  virtual void OnFirstLifecycleUnitCreated();

  // Called when all lifecycle units belonging to this source have been
  // destroyed. This is an empty stub which can optionally be overridden by
  // derived classes.
  virtual void OnAllLifecycleUnitsDestroyed();

 private:
  // Observers notified when a LifecycleUnit is created.
  base::ObserverList<LifecycleUnitSourceObserver>::UncheckedAndDanglingUntriaged
      observers_;

  // The count of lifecycle units associated with this source.
  size_t lifecycle_unit_count_ = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_SOURCE_BASE_H_
