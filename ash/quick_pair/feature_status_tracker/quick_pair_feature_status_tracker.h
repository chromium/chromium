// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace ash {
namespace quick_pair {

// Exposes APIs to query and track the status of the various
// Quick Pair implementations (e.g. Fast Pair).
class FeatureStatusTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnFastPairEnabledChanged(bool is_enabled) = 0;
  };

  virtual ~FeatureStatusTracker() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual bool IsFastPairEnabled() = 0;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_
