// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_SCANNING_MOCK_SCANNER_BROKER_H_
#define ASH_QUICK_PAIR_SCANNING_MOCK_SCANNER_BROKER_H_

#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace quick_pair {

class MockScannerBroker : public ScannerBroker {
 public:
  MockScannerBroker();
  MockScannerBroker(const MockScannerBroker&) = delete;
  MockScannerBroker& operator=(const MockScannerBroker&) = delete;
  ~MockScannerBroker() override;

  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  MOCK_METHOD(void, StartScanning, (Protocol), (override));
  MOCK_METHOD(void, StopScanning, (Protocol), (override));
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_SCANNING_MOCK_SCANNER_BROKER_H_
