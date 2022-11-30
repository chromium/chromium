// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"

#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"

namespace resource_coordinator {

LifecycleUnitSourceBase::LifecycleUnitSourceBase() = default;
LifecycleUnitSourceBase::~LifecycleUnitSourceBase() = default;

void LifecycleUnitSourceBase::AddObserver(
    LifecycleUnitSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void LifecycleUnitSourceBase::RemoveObserver(
    LifecycleUnitSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LifecycleUnitSourceBase::NotifyLifecycleUnitBeingCreated(
    LifecycleUnitBase* lifecycle_unit) {
  ++lifecycle_unit_count_;
  if (lifecycle_unit_count_ == 1)
    OnFirstLifecycleUnitCreated();
}

void LifecycleUnitSourceBase::NotifyLifecycleUnitCreated(
    LifecycleUnitBase* lifecycle_unit) {
  for (LifecycleUnitSourceObserver& observer : observers_) {
    observer.OnLifecycleUnitCreated(lifecycle_unit);
  }
}

void LifecycleUnitSourceBase::NotifyLifecycleUnitBeingDestroyed(
    LifecycleUnitBase* lifecycle_unit) {
  DCHECK_LT(0u, lifecycle_unit_count_);
  --lifecycle_unit_count_;
  if (lifecycle_unit_count_ == 0)
    OnAllLifecycleUnitsDestroyed();
}

void LifecycleUnitSourceBase::OnFirstLifecycleUnitCreated() {}

void LifecycleUnitSourceBase::OnAllLifecycleUnitsDestroyed() {}

}  // namespace resource_coordinator
