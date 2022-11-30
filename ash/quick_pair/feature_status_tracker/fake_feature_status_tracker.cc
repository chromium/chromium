// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fake_feature_status_tracker.h"

#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

FakeFeatureStatusTracker::FakeFeatureStatusTracker() = default;

FakeFeatureStatusTracker::~FakeFeatureStatusTracker() = default;

void FakeFeatureStatusTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeFeatureStatusTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeFeatureStatusTracker::IsFastPairEnabled() {
  return is_fast_pair_enabled_;
}

void FakeFeatureStatusTracker::SetIsFastPairEnabled(bool is_enabled) {
  is_fast_pair_enabled_ = is_enabled;

  for (auto& observer : observers_)
    observer.OnFastPairEnabledChanged(is_fast_pair_enabled_);
}

}  // namespace quick_pair
}  // namespace ash
