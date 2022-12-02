// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/initializers/keyboard_pref_initializer.h"

#include <memory>

#include "ash/test/ash_test_base.h"

namespace ash {

class KeyboardPrefInitializerTest : public AshTestBase {
 public:
  KeyboardPrefInitializerTest() = default;
  KeyboardPrefInitializerTest(const KeyboardPrefInitializerTest&) = delete;
  KeyboardPrefInitializerTest& operator=(const KeyboardPrefInitializerTest&) =
      delete;
  ~KeyboardPrefInitializerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    initializer_ = std::make_unique<KeyboardPrefInitializer>();
  }

  void TearDown() override {
    initializer_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<KeyboardPrefInitializer> initializer_;
};

TEST_F(KeyboardPrefInitializerTest, InitializationTest) {
  EXPECT_NE(initializer_.get(), nullptr);
}

}  // namespace ash
