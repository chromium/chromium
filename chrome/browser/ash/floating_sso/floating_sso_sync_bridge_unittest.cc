// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/test/mock_model_type_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::floating_sso {

namespace {

constexpr char kKeyForTests[] = "test_key_value";

}  // namespace

class FloatingSsoSyncBridgeTest : public testing::Test {
 public:
  void SetUp() override {
    bridge_ = std::make_unique<FloatingSsoSyncBridge>(
        processor_.CreateForwardingProcessor());
  }

  FloatingSsoSyncBridge& bridge() { return *bridge_; }

 private:
  testing::NiceMock<syncer::MockModelTypeChangeProcessor> processor_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<FloatingSsoSyncBridge> bridge_;
};

TEST_F(FloatingSsoSyncBridgeTest, GetStorageKey) {
  syncer::EntityData entity;
  entity.specifics.mutable_cookie()->set_unique_key(kKeyForTests);
  EXPECT_EQ(kKeyForTests, bridge().GetStorageKey(entity));
}

TEST_F(FloatingSsoSyncBridgeTest, GetClientTag) {
  syncer::EntityData entity;
  entity.specifics.mutable_cookie()->set_unique_key(kKeyForTests);
  EXPECT_EQ(kKeyForTests, bridge().GetClientTag(entity));
}

}  // namespace ash::floating_sso
