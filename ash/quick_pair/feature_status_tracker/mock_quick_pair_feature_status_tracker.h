// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_

#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockFeatureStatusTracker : public FeatureStatusTracker {
 public:
  MockFeatureStatusTracker();
  MockFeatureStatusTracker(const MockFeatureStatusTracker&) = delete;
  MockFeatureStatusTracker& operator=(const MockFeatureStatusTracker&) = delete;
  ~MockFeatureStatusTracker() override;

  MOCK_METHOD(void, AddObserver, (FeatureStatusTracker::Observer*), (override));

  MOCK_METHOD(void,
              RemoveObserver,
              (FeatureStatusTracker::Observer*),
              (override));

  MOCK_METHOD(bool, IsFastPairEnabled, (), (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_QUICK_PAIR_FEATURE_STATUS_TRACKER_H_
