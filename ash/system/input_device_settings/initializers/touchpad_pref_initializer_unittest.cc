// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/touchpad_pref_initializer.h"

#include <memory>

#include "ash/test/ash_test_base.h"

namespace ash {

class TouchpadPrefInitializerTest : public AshTestBase {
 public:
  TouchpadPrefInitializerTest() = default;
  TouchpadPrefInitializerTest(const TouchpadPrefInitializerTest&) = delete;
  TouchpadPrefInitializerTest& operator=(const TouchpadPrefInitializerTest&) =
      delete;
  ~TouchpadPrefInitializerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    initializer_ = std::make_unique<TouchpadPrefInitializer>();
  }

  void TearDown() override {
    initializer_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TouchpadPrefInitializer> initializer_;
};

TEST_F(TouchpadPrefInitializerTest, InitializationTest) {
  EXPECT_NE(initializer_.get(), nullptr);
}

}  // namespace ash
