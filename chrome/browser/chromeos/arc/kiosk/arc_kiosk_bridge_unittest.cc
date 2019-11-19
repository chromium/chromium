// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/kiosk/arc_kiosk_bridge.h"
#include "components/arc/session/arc_bridge_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockArcKioskBridgeDelegate : public arc::ArcKioskBridge::Delegate {
 public:
  MockArcKioskBridgeDelegate() = default;

  MOCK_METHOD0(OnMaintenanceSessionCreated, void());
  MOCK_METHOD0(OnMaintenanceSessionFinished, void());
};

}  // namespace

namespace arc {

class ArcKioskBridgeTest : public testing::Test {
 public:
  ArcKioskBridgeTest()
      : bridge_service_(std::make_unique<ArcBridgeService>()),
        delegate_(std::make_unique<MockArcKioskBridgeDelegate>()),
        kiosk_bridge_(ArcKioskBridge::CreateForTesting(bridge_service_.get(),
                                                       delegate_.get())) {}

 protected:
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<MockArcKioskBridgeDelegate> delegate_;
  std::unique_ptr<ArcKioskBridge> kiosk_bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcKioskBridgeTest);
};

TEST_F(ArcKioskBridgeTest, MaintenanceSessionFinished) {
  EXPECT_CALL(*delegate_, OnMaintenanceSessionCreated()).Times(1);
  kiosk_bridge_->OnMaintenanceSessionCreated(1);
  EXPECT_CALL(*delegate_, OnMaintenanceSessionFinished()).Times(1);
  kiosk_bridge_->OnMaintenanceSessionFinished(1, true);
}

TEST_F(ArcKioskBridgeTest, MaintenanceSessionNotFinished) {
  EXPECT_CALL(*delegate_, OnMaintenanceSessionCreated()).Times(1);
  kiosk_bridge_->OnMaintenanceSessionCreated(1);
  EXPECT_CALL(*delegate_, OnMaintenanceSessionFinished()).Times(0);
  kiosk_bridge_->OnMaintenanceSessionFinished(2, true);
}

}  // namespace arc
