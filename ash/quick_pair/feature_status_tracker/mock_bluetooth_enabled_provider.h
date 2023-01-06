// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BLUETOOTH_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BLUETOOTH_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"

#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockBluetoothEnabledProvider : public BluetoothEnabledProvider {
 public:
  MockBluetoothEnabledProvider();
  MockBluetoothEnabledProvider(const MockBluetoothEnabledProvider&) = delete;
  MockBluetoothEnabledProvider& operator=(const MockBluetoothEnabledProvider&) =
      delete;
  ~MockBluetoothEnabledProvider() override;

  MOCK_METHOD(bool, is_enabled, (), (override));
  MOCK_METHOD(void,
              SetCallback,
              (base::RepeatingCallback<void(bool)>),
              (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_MOCK_BLUETOOTH_ENABLED_PROVIDER_H_
