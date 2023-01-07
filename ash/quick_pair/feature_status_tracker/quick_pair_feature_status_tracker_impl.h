// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_IMPL_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_IMPL_H_

#include <memory>

#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

class FeatureStatusTrackerImpl : public FeatureStatusTracker {
 public:
  FeatureStatusTrackerImpl();
  ~FeatureStatusTrackerImpl() override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsFastPairEnabled() override;

 private:
  void OnFastPairEnabledChanged(bool is_enabled);

  base::ObserverList<Observer> observers_;
  std::unique_ptr<FastPairEnabledProvider> fast_pair_enabled_provider_;
  base::WeakPtrFactory<FeatureStatusTrackerImpl> weak_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_IMPL_H_
