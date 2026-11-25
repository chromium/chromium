// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hold_to_speak_controller.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/session_view_impl.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"
#include "chrome/browser/voice/hotkeys/hotkey_hold_watcher.h"
#include "chrome/browser/voice/hotkeys/hotkey_pref_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view.h"

namespace ttc {

namespace {

class TtcHoldToSpeakInteractiveUiTest : public TtcInteractiveBrowserTestBase {
 public:
  TtcHoldToSpeakInteractiveUiTest() {
    // Use a large threshold so scheduling delays cannot fire it before
    // ReachHoldThreshold() is called.
    hold_to_speak_feature_.InitAndEnableFeatureWithParameters(
        kTtcHoldToSpeak, {{kTtcHoldToSpeakThreshold.name, "1h"}});
  }

 protected:
  ui::Accelerator GetHotkey() {
    return voice::ParseHotkeyPref(
        profile()->GetPrefs()->GetString(prefs::kTtcHoldToSpeakShortcut));
  }

  HoldToSpeakController* hold_to_speak() {
    auto& controller =
        static_cast<SessionControllerImpl&>(session_controller());
    return static_cast<SessionViewImpl&>(controller.session_view())
        .hold_to_speak_controller_for_testing();
  }

  voice::HotkeyHoldWatcher* hold_watcher() {
    return hold_to_speak()->hold_watcher_for_testing();
  }

  bool IsHotkeyRegistered() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->GetFocusManager()
        ->IsAcceleratorRegistered(GetHotkey());
  }

  // Starts a session, makes it live and mutes or unmutes its microphone.
  StepBuilder StartLiveSession(bool mic_muted) {
    return Do([this, mic_muted]() {
      ttc_service().StartSession();
      session_controller().SetSessionLifecycle(SessionLifecycle::kLive);
      ttc_service().SetMicMuted(mic_muted);
    });
  }

  // Presses the hotkey, as the browser window's FocusManager does.
  StepBuilder PressHotkey() {
    return Do([this]() {
      ASSERT_TRUE(hold_to_speak());
      EXPECT_TRUE(hold_to_speak()->AcceleratorPressed(GetHotkey()));
      EXPECT_TRUE(hold_watcher());
    });
  }

  // Presses the hotkey through the voice plate's FocusManager, as when the
  // voice plate has focus, e.g. after a click on its mute button.
  StepBuilder PressHotkeyInVoicePlate() {
    return WithView(
        TtcVoicePlate::kViewElementId, [this](views::View* voice_plate) {
          EXPECT_TRUE(
              voice_plate->GetFocusManager()->ProcessAccelerator(GetHotkey()));
          EXPECT_TRUE(hold_watcher());
        });
  }

  // Checks that the hotkey is registered with the voice plate, which has its
  // own FocusManager.
  StepBuilder CheckVoicePlateHasHotkey() {
    return CheckView(
        TtcVoicePlate::kViewElementId, [this](views::View* voice_plate) {
          return voice_plate->GetFocusManager()->IsAcceleratorRegistered(
              GetHotkey());
        });
  }

  // Sends a key event to the hold watcher, as its event monitor does.
  StepBuilder SendKeyEvent(ui::EventType type, ui::KeyboardCode key_code) {
    return Do([this, type, key_code]() {
      ASSERT_TRUE(hold_watcher());
      hold_watcher()->OnEvent(
          ui::KeyEvent(type, key_code, GetHotkey().modifiers()));
    });
  }

  StepBuilder ReleaseHotkey() {
    return SendKeyEvent(ui::EventType::kKeyReleased, GetHotkey().key_code());
  }

  // Fires the hold watcher's threshold timer without waiting in real time.
  StepBuilder ReachHoldThreshold() {
    return Do([this]() {
      ASSERT_TRUE(hold_watcher());
      hold_watcher()->TriggerHoldThresholdForTesting();
    });
  }

  StepBuilder CheckMicMuted(bool expected_muted) {
    return CheckResult([this]() { return ttc_service().IsMicMuted(); },
                       expected_muted);
  }

  StepBuilder CheckHoldWatched(bool expected_watched) {
    return CheckResult([this]() { return hold_watcher() != nullptr; },
                       expected_watched);
  }

 private:
  base::test::ScopedFeatureList hold_to_speak_feature_;
};

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       HotkeyIsOnlyRegisteredDuringSession) {
  // clang-format off
  RunTestSequence(
    CheckResult([this]() { return IsHotkeyRegistered(); }, false),
    Do([this]() { ttc_service().StartSession(); }),
    CheckResult([this]() { return IsHotkeyRegistered(); }, true),
    Do([this]() { ttc_service().EndSession(); }),
    CheckResult([this]() { return IsHotkeyRegistered(); }, false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       HoldTurnsMicOnUntilRelease) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/true),
    PressHotkey(),
    // Nothing happens until the hotkey has been held for the threshold.
    CheckMicMuted(true),
    ReachHoldThreshold(),
    CheckMicMuted(false),
    ReleaseHotkey(),
    CheckHoldWatched(false),
    CheckMicMuted(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       ReleaseTurnsOffMicThatWasAlreadyOn) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/false),
    PressHotkey(),
    ReachHoldThreshold(),
    CheckMicMuted(false),
    ReleaseHotkey(),
    CheckMicMuted(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest, TapLeavesMicMuted) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/true),
    PressHotkey(),
    ReleaseHotkey(),
    CheckHoldWatched(false),
    CheckMicMuted(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest, TapLeavesMicOn) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/false),
    PressHotkey(),
    ReleaseHotkey(),
    CheckHoldWatched(false),
    CheckMicMuted(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       InterruptedHoldTurnsMicOff) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/true),
    PressHotkey(),
    ReachHoldThreshold(),
    CheckMicMuted(false),
    // Pressing another key interrupts the hold.
    SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A),
    CheckHoldWatched(false),
    CheckMicMuted(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       HoldWorksWhileVoicePlateHasFocus) {
  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    StartLiveSession(/*mic_muted=*/true),
    WaitForShow(TtcVoicePlate::kViewElementId),
    PressHotkeyInVoicePlate(),
    ReachHoldThreshold(),
    CheckMicMuted(false),
    ReleaseHotkey(),
    CheckHoldWatched(false),
    CheckMicMuted(true)
  );
  // clang-format on
}

// The voice plate is recreated in each window it moves to.
IN_PROC_BROWSER_TEST_F(TtcHoldToSpeakInteractiveUiTest,
                       HotkeyFollowsVoicePlateToNewWindow) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this]() { ttc_service().StartSession(); }),
    WaitForShow(TtcVoicePlate::kViewElementId),
    CheckVoicePlateHasHotkey()
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());

  // CreateBrowser() doesn't wait for the new window to be activated, and only
  // activation moves the voice plate, so activate it explicitly.
  // clang-format off
  RunTestSequence(
    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(TtcVoicePlate::kViewElementId)),
    InContext(BrowserElements::From(second_browser)->GetContext(),
              CheckVoicePlateHasHotkey())
  );
  // clang-format on
}

}  // namespace

}  // namespace ttc
