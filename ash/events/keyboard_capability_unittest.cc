// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

class TestObserver : public ui::KeyboardCapability::Observer {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  void OnTopRowKeysAreFKeysChanged() override {
    ++top_row_keys_are_f_keys_changed_count_;
  }

  int top_row_keys_are_f_keys_changed_count() {
    return top_row_keys_are_f_keys_changed_count_;
  }

 private:
  int top_row_keys_are_f_keys_changed_count_ = 0;
};

}  // namespace

class KeyboardCapabilityTest : public AshTestBase {
 public:
  KeyboardCapabilityTest() = default;
  ~KeyboardCapabilityTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    keyboard_capability_ = Shell::Get()->keyboard_capability();
    test_observer_ = std::make_unique<TestObserver>();
    keyboard_capability_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    keyboard_capability_->RemoveObserver(test_observer_.get());
    AshTestBase::TearDown();
  }

 protected:
  ui::KeyboardCapability* keyboard_capability_;
  std::unique_ptr<TestObserver> test_observer_;
};

TEST_F(KeyboardCapabilityTest, TestObserver) {
  EXPECT_EQ(0, test_observer_->top_row_keys_are_f_keys_changed_count());
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  prefs->SetBoolean(prefs::kSendFunctionKeys, true);
  prefs->CommitPendingWrite();

  EXPECT_TRUE(keyboard_capability_->TopRowKeysAreFKeys());
  EXPECT_EQ(1, test_observer_->top_row_keys_are_f_keys_changed_count());

  prefs->SetBoolean(prefs::kSendFunctionKeys, false);
  prefs->CommitPendingWrite();

  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
  EXPECT_EQ(2, test_observer_->top_row_keys_are_f_keys_changed_count());
}

TEST_F(KeyboardCapabilityTest, TestTopRowKeysAreFKeys) {
  // Top row keys are F-Keys pref is false in default.
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());

  keyboard_capability_->SetTopRowKeysAsFKeysEnabledForTesting(true);
  EXPECT_TRUE(keyboard_capability_->TopRowKeysAreFKeys());

  keyboard_capability_->SetTopRowKeysAsFKeysEnabledForTesting(false);
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
}

TEST_F(KeyboardCapabilityTest, TestIsSixPackKey) {
  for (const auto& [key_code, _] : ui::kSixPackKeyToSystemKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsSixPackKey(key_code));
  }

  // A key not in the kSixPackKeyList is not a six pack key.
  EXPECT_FALSE(keyboard_capability_->IsSixPackKey(ui::KeyboardCode::VKEY_A));
}

TEST_F(KeyboardCapabilityTest, TestIsTopRowKey) {
  for (const auto& [key_code, _] : ui::kLayout2TopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }

  // A key not in the kLayout2TopRowKeyToFKeyMap is not a top row key.
  EXPECT_FALSE(keyboard_capability_->IsTopRowKey(ui::KeyboardCode::VKEY_A));
}

}  // namespace ash
