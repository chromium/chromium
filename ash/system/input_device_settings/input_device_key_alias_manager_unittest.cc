// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_key_alias_manager.h"

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDeviceKeyAliasManagerTest : public AshTestBase {
 public:
  InputDeviceKeyAliasManagerTest() = default;
  InputDeviceKeyAliasManagerTest(const InputDeviceKeyAliasManagerTest&) =
      delete;
  InputDeviceKeyAliasManagerTest& operator=(
      const InputDeviceKeyAliasManagerTest&) = delete;
  ~InputDeviceKeyAliasManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceKeyAliasManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceKeyAliasManager> manager_;
};

TEST_F(InputDeviceKeyAliasManagerTest, InitializationTest) {
  EXPECT_NE(manager_.get(), nullptr);
}

}  // namespace ash
