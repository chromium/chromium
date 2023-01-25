// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_floss_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace arc {

class ArcFlossBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    // TODO: Use Singleton instance tied to BrowserContext.
    arc_bluetooth_bridge_ =
        std::make_unique<ArcFlossBridge>(nullptr, arc_bridge_service_.get());
  }

  void TearDown() override {
    arc_bluetooth_bridge_.reset();
    arc_bridge_service_.reset();
  }

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcBluetoothBridge> arc_bluetooth_bridge_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(ArcFlossBridgeTest, Noop) {}

}  // namespace arc
