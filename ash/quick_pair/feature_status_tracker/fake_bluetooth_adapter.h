// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_

#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  void NotifyPoweredChanged(bool powered) {
    device::BluetoothAdapter::NotifyAdapterPoweredChanged(powered);
  }

 private:
  ~FakeBluetoothAdapter() = default;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAKE_BLUETOOTH_ADAPTER_H_
