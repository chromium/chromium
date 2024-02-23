// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BATTERY_SAVER_ACTIVE_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BATTERY_SAVER_ACTIVE_PROVIDER_H_

#include "testing/gmock/include/gmock/gmock.h"

#include "ash/quick_pair/feature_status_tracker/battery_saver_active_provider.h"

namespace ash::quick_pair {

class MockBatterySaverActiveProvider : public BatterySaverActiveProvider {
 public:
  MockBatterySaverActiveProvider();
  MockBatterySaverActiveProvider(const MockBatterySaverActiveProvider&) =
      delete;
  MockBatterySaverActiveProvider& operator=(
      const MockBatterySaverActiveProvider&) = delete;
  ~MockBatterySaverActiveProvider() override;

  MOCK_METHOD(bool, is_enabled, (), (override));
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BATTERY_SAVER_ACTIVE_PROVIDER_H_
