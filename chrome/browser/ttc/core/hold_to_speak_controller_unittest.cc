// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hold_to_speak_controller.h"

#include <memory>

#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ui/views/voice/hotkeys/fake_hotkey_registry.h"
#include "chrome/browser/ui/views/voice/hotkeys/hotkey_registration_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ttc {

namespace {

class TtcHoldToSpeakControllerTest : public testing::Test {
 public:
  TtcHoldToSpeakControllerTest() { delegate_.set_profile(&profile_); }

 protected:
  void CreateController() {
    controller_ = std::make_unique<HoldToSpeakController>(
        delegate_, browser_registry_.CreateDelegate(),
        *overlay_registration_delegate_);
  }

  PrefService& prefs() { return *profile_.GetPrefs(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeSessionViewDelegate delegate_;
  voice::FakeHotkeyRegistry browser_registry_;
  voice::FakeHotkeyRegistry overlay_registry_;
  const std::unique_ptr<voice::HotkeyRegistrationDelegate>
      overlay_registration_delegate_ = overlay_registry_.CreateDelegate();
  std::unique_ptr<HoldToSpeakController> controller_;
};

TEST_F(TtcHoldToSpeakControllerTest, RegistersDefaultHotkey) {
  CreateController();
  const ui::Accelerator default_hotkey(ui::VKEY_SPACE,
                                       ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(browser_registry_.is_registered());
  EXPECT_EQ(browser_registry_.last_registered_accelerator(), default_hotkey);
  EXPECT_TRUE(overlay_registry_.is_registered());
  EXPECT_EQ(overlay_registry_.last_registered_accelerator(), default_hotkey);
}

TEST_F(TtcHoldToSpeakControllerTest, EmptyPrefDisablesHotkey) {
  prefs().SetString(prefs::kTtcHoldToSpeakShortcut, "");
  CreateController();
  EXPECT_FALSE(browser_registry_.is_registered());
  EXPECT_FALSE(overlay_registry_.is_registered());
}

TEST_F(TtcHoldToSpeakControllerTest, PrefChangeReregistersHotkey) {
  CreateController();
  const ui::Accelerator custom_hotkey(ui::VKEY_T,
                                      ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  prefs().SetString(prefs::kTtcHoldToSpeakShortcut,
                    ui::Command::AcceleratorToString(custom_hotkey));
  EXPECT_TRUE(browser_registry_.is_registered());
  EXPECT_EQ(browser_registry_.last_registered_accelerator(), custom_hotkey);
  EXPECT_EQ(browser_registry_.registration_count(), 2);
  EXPECT_EQ(browser_registry_.destruction_count(), 1);
  EXPECT_TRUE(overlay_registry_.is_registered());
  EXPECT_EQ(overlay_registry_.last_registered_accelerator(), custom_hotkey);
  EXPECT_EQ(overlay_registry_.registration_count(), 2);
  EXPECT_EQ(overlay_registry_.destruction_count(), 1);
}

TEST_F(TtcHoldToSpeakControllerTest, UnregistersHotkeyOnDestruction) {
  CreateController();
  controller_.reset();
  EXPECT_FALSE(browser_registry_.is_registered());
  EXPECT_FALSE(overlay_registry_.is_registered());
}

TEST_F(TtcHoldToSpeakControllerTest, IgnoresHotkeyUntilSessionIsLive) {
  delegate_.set_session_lifecycle(SessionLifecycle::kInitializing);
  delegate_.set_app_mic_muted(true);
  CreateController();
  EXPECT_TRUE(controller_->AcceleratorPressed(
      *browser_registry_.last_registered_accelerator()));
  EXPECT_FALSE(controller_->hold_watcher_for_testing());
  EXPECT_TRUE(delegate_.IsMicMuted());
}

TEST_F(TtcHoldToSpeakControllerTest, DoesNotHandleOtherAccelerators) {
  CreateController();
  EXPECT_FALSE(controller_->AcceleratorPressed(
      ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN)));
}

}  // namespace

}  // namespace ttc
