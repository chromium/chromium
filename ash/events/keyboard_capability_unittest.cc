// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include "ash/test/ash_test_base.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

constexpr ui::KeyboardCode kSixPackKeyList[] = {
    ui::KeyboardCode::VKEY_DELETE, ui::KeyboardCode::VKEY_HOME,
    ui::KeyboardCode::VKEY_UP,     ui::KeyboardCode::VKEY_END,
    ui::KeyboardCode::VKEY_NEXT,   ui::KeyboardCode::VKEY_INSERT,
};

constexpr ui::KeyboardCode kLegacyLayoutTwoTopRowKeyList[] = {
    ui::KeyboardCode::VKEY_BROWSER_BACK,
    ui::KeyboardCode::VKEY_BROWSER_REFRESH,
    ui::KeyboardCode::VKEY_ZOOM,
    ui::KeyboardCode::VKEY_MEDIA_LAUNCH_APP1,
    ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN,
    ui::KeyboardCode::VKEY_BRIGHTNESS_UP,
    ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE,
    ui::KeyboardCode::VKEY_VOLUME_MUTE,
    ui::KeyboardCode::VKEY_VOLUME_DOWN,
    ui::KeyboardCode::VKEY_VOLUME_UP,
};

class KeyboardCapabilityTest : public AshTestBase {
 public:
  KeyboardCapabilityTest() = default;
  ~KeyboardCapabilityTest() override = default;

  void SetUp() override {
    keyboard_capability_ = std::make_unique<ui::KeyboardCapability>();
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    keyboard_capability_.reset();
  }

 protected:
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
};

TEST_F(KeyboardCapabilityTest, TestIsSixPackKey) {
  for (const ui::KeyboardCode key_code : kSixPackKeyList) {
    EXPECT_TRUE(keyboard_capability_->IsSixPackKey(key_code));
  }

  // A key not in the kSixPackKeyList is not a six pack key.
  EXPECT_FALSE(keyboard_capability_->IsSixPackKey(ui::KeyboardCode::VKEY_A));
}

TEST_F(KeyboardCapabilityTest, TestIsTopRowKey) {
  for (const ui::KeyboardCode key_code : kLegacyLayoutTwoTopRowKeyList) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }

  // A key not in the kLegacyLayoutTwoTopRowKeyList is not a top row key.
  EXPECT_FALSE(keyboard_capability_->IsTopRowKey(ui::KeyboardCode::VKEY_A));
}

}  // namespace ash
