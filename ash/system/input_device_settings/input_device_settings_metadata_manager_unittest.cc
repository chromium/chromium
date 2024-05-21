// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"

#include <memory>

#include "ash/test/ash_test_base.h"

namespace ash {

class InputDeviceSettingsMetadataManagerTest : public AshTestBase {
 public:
  InputDeviceSettingsMetadataManagerTest() = default;
  InputDeviceSettingsMetadataManagerTest(
      const InputDeviceSettingsMetadataManagerTest&) = delete;
  InputDeviceSettingsMetadataManagerTest& operator=(
      const InputDeviceSettingsMetadataManagerTest&) = delete;
  ~InputDeviceSettingsMetadataManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceSettingsMetadataManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsMetadataManager> manager_;
};

// TODO(b/329686601): Remove this stub test.
TEST_F(InputDeviceSettingsMetadataManagerTest, Initialize) {
  EXPECT_NE(manager_.get(), nullptr);
}

}  // namespace ash
