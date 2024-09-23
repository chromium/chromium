// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCANNING_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCANNING_ENABLED_PROVIDER_H_

#include "testing/gmock/include/gmock/gmock.h"

#include "ash/quick_pair/feature_status_tracker/scanning_enabled_provider.h"

namespace ash::quick_pair {

class MockScanningEnabledProvider : public ScanningEnabledProvider {
 public:
  MockScanningEnabledProvider();

  MockScanningEnabledProvider(const MockScanningEnabledProvider&) = delete;
  MockScanningEnabledProvider& operator=(const MockScanningEnabledProvider&) =
      delete;
  ~MockScanningEnabledProvider() override;

  MOCK_METHOD(bool, is_enabled, (), (override));
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_SCANNING_ENABLED_PROVIDER_H_
