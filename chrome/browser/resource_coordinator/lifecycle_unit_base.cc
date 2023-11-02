// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/usage_clock.h"

namespace resource_coordinator {

LifecycleUnitBase::LifecycleUnitBase(LifecycleUnitSourceBase* source,
                                     content::Visibility visibility,
                                     UsageClock* usage_clock)
    : source_(source),
      wall_time_when_hidden_(visibility == content::Visibility::VISIBLE
                                 ? base::TimeTicks::Max()
                                 : NowTicks()),
      usage_clock_(usage_clock),
      chrome_usage_time_when_hidden_(visibility == content::Visibility::VISIBLE
                                         ? base::TimeDelta::Max()
                                         : usage_clock_->GetTotalUsageTime()) {
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

base::TimeTicks LifecycleUnitBase::GetWallTimeWhenHidden() const {
  return wall_time_when_hidden_;
}

base::TimeDelta LifecycleUnitBase::GetChromeUsageTimeWhenHidden() const {
  return chrome_usage_time_when_hidden_;
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

ukm::SourceId LifecycleUnitBase::GetUkmSourceId() const {
  return ukm::kInvalidSourceId;
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
  OnLifecycleUnitStateChanged(last_state, reason);
  for (auto& observer : observers_)
    observer.OnLifecycleUnitStateChanged(this, last_state, reason);
}

void LifecycleUnitBase::OnLifecycleUnitStateChanged(
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {}

void LifecycleUnitBase::OnLifecycleUnitVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    wall_time_when_hidden_ = base::TimeTicks::Max();
    chrome_usage_time_when_hidden_ = base::TimeDelta::Max();
  } else if (wall_time_when_hidden_.is_max()) {
    DCHECK(chrome_usage_time_when_hidden_.is_max());
    wall_time_when_hidden_ = NowTicks();
    chrome_usage_time_when_hidden_ = usage_clock_->GetTotalUsageTime();
  }

  for (auto& observer : observers_)
    observer.OnLifecycleUnitVisibilityChanged(this, visibility);
}

void LifecycleUnitBase::OnLifecycleUnitDestroyed() {
  for (auto& observer : observers_)
    observer.OnLifecycleUnitDestroyed(this);
}

int32_t LifecycleUnitBase::next_id_ = 0;

}  // namespace resource_coordinator
