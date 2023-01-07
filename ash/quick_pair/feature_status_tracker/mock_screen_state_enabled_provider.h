// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCREEN_STATE_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCREEN_STATE_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockScreenStateEnabledProvider : public ScreenStateEnabledProvider {
 public:
  MockScreenStateEnabledProvider();
  MockScreenStateEnabledProvider(const MockScreenStateEnabledProvider&) =
      delete;
  MockScreenStateEnabledProvider& operator=(
      const MockScreenStateEnabledProvider&) = delete;
  ~MockScreenStateEnabledProvider() override;

  MOCK_METHOD(bool, is_enabled, (), (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCREEN_STATE_ENABLED_PROVIDER_H_