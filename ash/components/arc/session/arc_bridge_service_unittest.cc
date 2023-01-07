// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_bridge_service.h"

#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcBridgeServiceTest : public testing::Test,
                             public ArcBridgeService::Observer {
 public:
  ArcBridgeServiceTest() = default;
  ArcBridgeServiceTest(const ArcBridgeServiceTest&) = delete;
  ArcBridgeServiceTest& operator=(const ArcBridgeServiceTest&) = delete;
  ~ArcBridgeServiceTest() override = default;

  // ArcBridgeService::Observer overrides:
  void BeforeArcBridgeClosed() override { ++num_before_called_; }
  void AfterArcBridgeClosed() override { ++num_after_called_; }

 protected:
  ArcBridgeService bridge_;
  size_t num_before_called_ = 0;
  size_t num_after_called_ = 0;
};

TEST_F(ArcBridgeServiceTest, Observers) {
  base::ScopedObservation<ArcBridgeService, ArcBridgeService::Observer>
      bridge_observation(this);
  bridge_observation.Observe(&bridge_);
  EXPECT_EQ(0u, num_before_called_);
  bridge_.ObserveBeforeArcBridgeClosed();
  EXPECT_EQ(1u, num_before_called_);
  EXPECT_EQ(0u, num_after_called_);
  bridge_.ObserveAfterArcBridgeClosed();
  EXPECT_EQ(1u, num_before_called_);  // this should not change
  EXPECT_EQ(1u, num_after_called_);
}

}  // namespace
}  // namespace arc
