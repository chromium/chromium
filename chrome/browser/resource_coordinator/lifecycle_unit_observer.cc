// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"

namespace resource_coordinator {

LifecycleUnitObserver::~LifecycleUnitObserver() = default;

void LifecycleUnitObserver::OnLifecycleUnitStateChanged(
    LifecycleUnit* lifecycle_unit,
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {}

void LifecycleUnitObserver::OnLifecycleUnitVisibilityChanged(
    LifecycleUnit* lifecycle_unit,
    content::Visibility visibility) {}

void LifecycleUnitObserver::OnLifecycleUnitDestroyed(
    LifecycleUnit* lifecycle_unit) {}

}  // namespace resource_coordinator
