// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_FEATURE_STATUS_TRACKER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_FEATURE_STATUS_TRACKER_H_

#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "base/observer_list.h"

namespace ash {
namespace quick_pair {

class FakeFeatureStatusTracker : public FeatureStatusTracker {
 public:
  FakeFeatureStatusTracker();
  ~FakeFeatureStatusTracker() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  bool IsFastPairEnabled() override;
  void SetIsFastPairEnabled(bool is_enabled);

 private:
  bool is_fast_pair_enabled_ = false;
  base::ObserverList<Observer> observers_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_FEATURE_STATUS_TRACKER_H_
