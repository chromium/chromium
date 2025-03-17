// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

LifecycleUnitBase::LifecycleUnitBase(LifecycleUnitSourceBase* source)
    : source_(source) {
  if (source_)
    source_->NotifyLifecycleUnitBeingCreated(this);
}

LifecycleUnitBase::~LifecycleUnitBase() {
  if (source_)
    source_->NotifyLifecycleUnitBeingDestroyed(this);
}

LifecycleUnitSource* LifecycleUnitBase::GetSource() const {
  return source_;
}

int32_t LifecycleUnitBase::GetID() const {
  return id_;
}

LifecycleUnitState LifecycleUnitBase::GetState() const {
  return state_;
}

base::TimeTicks LifecycleUnitBase::GetStateChangeTime() const {
  return state_change_time_;
}

size_t LifecycleUnitBase::GetDiscardCount() const {
  return discard_count_;
}

void LifecycleUnitBase::AddObserver(LifecycleUnitObserver* observer) {
  observers_.AddObserver(observer);
}

void LifecycleUnitBase::RemoveObserver(LifecycleUnitObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LifecycleUnitBase::SetDiscardCountForTesting(size_t discard_count) {
  discard_count_ = discard_count;
}

void LifecycleUnitBase::SetState(LifecycleUnitState state,
                                 LifecycleUnitStateChangeReason reason) {
  if (state == state_)
    return;

  // Only increment the discard count once the discard has actually completed.
  if (state == LifecycleUnitState::DISCARDED)
    ++discard_count_;

  LifecycleUnitState last_state = state_;
  state_ = state;
  state_change_time_ = NowTicks();
  for (auto& observer : observers_)
    observer.OnLifecycleUnitStateChanged(this, last_state, reason);
}

void LifecycleUnitBase::OnLifecycleUnitDestroyed() {
  for (auto& observer : observers_)
    observer.OnLifecycleUnitDestroyed(this);
}

int32_t LifecycleUnitBase::next_id_ = 0;

}  // namespace resource_coordinator
