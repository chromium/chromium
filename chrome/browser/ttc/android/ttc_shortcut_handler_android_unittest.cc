// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/android/ttc_shortcut_handler_android.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ttc {

namespace {

using ::testing::InSequence;
using ::testing::StrictMock;

constexpr int kAltShift = ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;
constexpr int kCtrlShift = ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN;

// The default shortcuts.
constexpr ui::Accelerator kAltShiftK(ui::VKEY_K, kAltShift);
constexpr ui::Accelerator kAltShiftSpace(ui::VKEY_SPACE, kAltShift);

ui::Accelerator Released(ui::KeyboardCode key_code, int modifiers) {
  return ui::Accelerator(key_code, modifiers,
                         ui::Accelerator::KeyState::RELEASED);
}

class MockDelegate : public TtcShortcutHandlerAndroid::Delegate {
 public:
  MOCK_METHOD(void, OnKeyboardShortcut, (), (override));
  MOCK_METHOD(void, OnHoldToSpeakPressed, (), (override));
  MOCK_METHOD(void, OnHoldToSpeakReleased, (), (override));
};

// Registers with a ui::AcceleratorManager, which ui::AcceleratorManagerAndroid
// forwards to, so that tests can send it key presses and releases.
class FakeAcceleratorRegistry
    : public TtcShortcutHandlerAndroid::AcceleratorRegistry {
 public:
  explicit FakeAcceleratorRegistry(ui::AcceleratorManager* manager)
      : manager_(manager) {}

  // TtcShortcutHandlerAndroid::AcceleratorRegistry:
  bool Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target) override {
    manager_->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        target);
    return true;
  }
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target) override {
    manager_->Unregister(accelerator, target);
  }

 private:
  const raw_ptr<ui::AcceleratorManager> manager_;
};

class TtcShortcutHandlerAndroidTest : public testing::Test {
 protected:
  TtcShortcutHandlerAndroidTest() {
    RegisterProfilePrefs(prefs_.registry());
    auto delegate = std::make_unique<StrictMock<MockDelegate>>();
    delegate_ = delegate.get();
    handler_ = std::make_unique<TtcShortcutHandlerAndroid>(
        std::move(delegate),
        std::make_unique<FakeAcceleratorRegistry>(&manager_), &prefs_);
  }

  void DestroyHandler() {
    delegate_ = nullptr;
    handler_.reset();
  }

  // Sends a key press or release, as AcceleratorManagerAndroid does. Returns
  // whether it was handled.
  bool Send(const ui::Accelerator& accelerator) {
    return manager_.Process(accelerator);
  }

  void SetKeyboardShortcut(const char* value) {
    prefs_.SetString(prefs::kTtcKeyboardShortcut, value);
  }
  void SetHoldToSpeakShortcut(const char* value) {
    prefs_.SetString(prefs::kTtcHoldToSpeakShortcut, value);
  }

  TestingPrefServiceSimple prefs_;
  ui::AcceleratorManager manager_;
  std::unique_ptr<TtcShortcutHandlerAndroid> handler_;
  raw_ptr<StrictMock<MockDelegate>> delegate_ = nullptr;
};

TEST_F(TtcShortcutHandlerAndroidTest, KeyboardShortcut) {
  EXPECT_CALL(*delegate_, OnKeyboardShortcut());
  EXPECT_TRUE(Send(kAltShiftK));
  // Only presses toggle the session.
  EXPECT_FALSE(Send(Released(ui::VKEY_K, kAltShift)));
}

TEST_F(TtcShortcutHandlerAndroidTest, HoldToSpeak) {
  {
    InSequence sequence;
    EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
    EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
  }
  EXPECT_TRUE(Send(kAltShiftSpace));
  EXPECT_TRUE(Send(Released(ui::VKEY_SPACE, kAltShift)));
}

TEST_F(TtcShortcutHandlerAndroidTest, HoldToSpeakReleaseOnlyHandledWhileHeld) {
  // Space keeps working everywhere else.
  EXPECT_FALSE(manager_.IsRegistered(Released(ui::VKEY_SPACE, 0)));
  EXPECT_FALSE(Send(Released(ui::VKEY_SPACE, 0)));
  EXPECT_FALSE(Send(Released(ui::VKEY_SPACE, kAltShift)));

  EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
  EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
  Send(kAltShiftSpace);
  Send(Released(ui::VKEY_SPACE, kAltShift));

  EXPECT_FALSE(manager_.IsRegistered(Released(ui::VKEY_SPACE, 0)));
  EXPECT_FALSE(manager_.IsRegistered(Released(ui::VKEY_SPACE, kAltShift)));
  EXPECT_FALSE(Send(Released(ui::VKEY_SPACE, 0)));
}

TEST_F(TtcShortcutHandlerAndroidTest, HoldToSpeakModifiersReleasedFirst) {
  for (int modifiers : {kAltShift, ui::EF_ALT_DOWN, ui::EF_SHIFT_DOWN, 0}) {
    SCOPED_TRACE(modifiers);
    EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
    EXPECT_TRUE(Send(kAltShiftSpace));
    testing::Mock::VerifyAndClearExpectations(delegate_);

    EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
    EXPECT_TRUE(Send(Released(ui::VKEY_SPACE, modifiers)));
    testing::Mock::VerifyAndClearExpectations(delegate_);
  }
}

TEST_F(TtcShortcutHandlerAndroidTest, HoldToSpeakPressedAgainWhileHeld) {
  EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
  EXPECT_TRUE(Send(kAltShiftSpace));
  // Already held, so this is still the same hold.
  EXPECT_TRUE(Send(kAltShiftSpace));
  testing::Mock::VerifyAndClearExpectations(delegate_);

  EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
  EXPECT_TRUE(Send(Released(ui::VKEY_SPACE, kAltShift)));
}

TEST_F(TtcShortcutHandlerAndroidTest, FollowsPrefChanges) {
  SetKeyboardShortcut("Ctrl+Shift+Y");
  SetHoldToSpeakShortcut("Alt+Shift+K");
  EXPECT_FALSE(manager_.IsRegistered(kAltShiftSpace));

  EXPECT_CALL(*delegate_, OnKeyboardShortcut());
  EXPECT_TRUE(Send(ui::Accelerator(ui::VKEY_Y, kCtrlShift)));
  testing::Mock::VerifyAndClearExpectations(delegate_);

  {
    InSequence sequence;
    EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
    EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
  }
  EXPECT_TRUE(Send(kAltShiftK));
  EXPECT_TRUE(Send(Released(ui::VKEY_K, ui::EF_ALT_DOWN)));
}

TEST_F(TtcShortcutHandlerAndroidTest, PrefChangeEndsHold) {
  EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
  Send(kAltShiftSpace);
  testing::Mock::VerifyAndClearExpectations(delegate_);

  EXPECT_CALL(*delegate_, OnHoldToSpeakReleased());
  SetHoldToSpeakShortcut("Ctrl+Shift+Y");
  testing::Mock::VerifyAndClearExpectations(delegate_);

  // The old shortcut's release isn't registered anymore.
  EXPECT_FALSE(Send(Released(ui::VKEY_SPACE, kAltShift)));
  EXPECT_FALSE(Send(Released(ui::VKEY_SPACE, 0)));
  EXPECT_FALSE(Send(kAltShiftSpace));
}

TEST_F(TtcShortcutHandlerAndroidTest, ClearedShortcuts) {
  SetKeyboardShortcut("");
  SetHoldToSpeakShortcut("");
  EXPECT_TRUE(manager_.IsEmpty());
}

TEST_F(TtcShortcutHandlerAndroidTest, InvalidShortcuts) {
  SetKeyboardShortcut("Shift+K");
  SetHoldToSpeakShortcut("not a shortcut");
  EXPECT_TRUE(manager_.IsEmpty());
}

TEST_F(TtcShortcutHandlerAndroidTest, SameShortcutForBothIsKeyboardShortcut) {
  SetHoldToSpeakShortcut("Alt+Shift+K");

  EXPECT_CALL(*delegate_, OnKeyboardShortcut());
  EXPECT_TRUE(Send(kAltShiftK));
  EXPECT_FALSE(Send(Released(ui::VKEY_K, kAltShift)));
  EXPECT_FALSE(manager_.IsRegistered(kAltShiftSpace));
}

TEST_F(TtcShortcutHandlerAndroidTest, DestroyUnregistersShortcuts) {
  EXPECT_CALL(*delegate_, OnHoldToSpeakPressed());
  Send(kAltShiftSpace);
  EXPECT_FALSE(manager_.IsEmpty());

  DestroyHandler();
  EXPECT_TRUE(manager_.IsEmpty());
}

}  // namespace

}  // namespace ttc
