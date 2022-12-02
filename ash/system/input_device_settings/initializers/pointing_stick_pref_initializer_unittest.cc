// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/pointing_stick_pref_initializer.h"

#include <memory>

#include "ash/test/ash_test_base.h"

namespace ash {

class PointingStickPrefInitializerTest : public AshTestBase {
 public:
  PointingStickPrefInitializerTest() = default;
  PointingStickPrefInitializerTest(const PointingStickPrefInitializerTest&) =
      delete;
  PointingStickPrefInitializerTest& operator=(
      const PointingStickPrefInitializerTest&) = delete;
  ~PointingStickPrefInitializerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    initializer_ = std::make_unique<PointingStickPrefInitializer>();
  }

  void TearDown() override {
    initializer_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<PointingStickPrefInitializer> initializer_;
};

TEST_F(PointingStickPrefInitializerTest, InitializationTest) {
  EXPECT_NE(initializer_.get(), nullptr);
}

}  // namespace ash
