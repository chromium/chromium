// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/mouse_pref_initializer.h"

#include <memory>

#include "ash/test/ash_test_base.h"

namespace ash {

class MousePrefInitializerTest : public AshTestBase {
 public:
  MousePrefInitializerTest() = default;
  MousePrefInitializerTest(const MousePrefInitializerTest&) = delete;
  MousePrefInitializerTest& operator=(const MousePrefInitializerTest&) = delete;
  ~MousePrefInitializerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    initializer_ = std::make_unique<MousePrefInitializer>();
  }

  void TearDown() override {
    initializer_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<MousePrefInitializer> initializer_;
};

TEST_F(MousePrefInitializerTest, InitializationTest) {
  EXPECT_NE(initializer_.get(), nullptr);
}

}  // namespace ash
