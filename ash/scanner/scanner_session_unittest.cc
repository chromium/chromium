// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_session.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class MockScannerSessionObserver : public ScannerSession::Observer {
 public:
  MockScannerSessionObserver() = default;
  MockScannerSessionObserver(const MockScannerSessionObserver&) = delete;
  MockScannerSessionObserver& operator=(const MockScannerSessionObserver&) =
      delete;
  ~MockScannerSessionObserver() override = default;

  MOCK_METHOD(void, OnScannerSessionDestroying, (), (override));
};

TEST(ScannerSessionTest, NotifiesObserversWhenDestroying) {
  MockScannerSessionObserver scanner_session_observer;
  auto scanner_session = std::make_unique<ScannerSession>(/*delegate=*/nullptr);
  scanner_session->AddObserver(&scanner_session_observer);

  EXPECT_CALL(scanner_session_observer, OnScannerSessionDestroying()).Times(1);

  scanner_session = nullptr;
}

}  // namespace

}  // namespace ash
