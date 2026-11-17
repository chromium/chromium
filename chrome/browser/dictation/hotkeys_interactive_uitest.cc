// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/hotkey_hold_watcher.h"
#include "chrome/browser/dictation/hotkey_manager.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "url/gurl.h"

namespace dictation {

class DictationHotkeysInteractiveUiTest
    : public DictationInteractiveBrowserTestBase {
 protected:
  // Centralizes simulated hotkey entry points for toggle and press-and-hold
  // tests.
  class HotkeySimulator {
   public:
    HotkeySimulator(DictationHotkeysInteractiveUiTest& test,
                    ui::Accelerator toggle,
                    ui::Accelerator hold)
        : test_(test), toggle_(toggle), hold_(hold) {}

    std::string toggle_string() const {
      return toggle_.IsEmpty() ? std::string()
                               : ui::Command::AcceleratorToString(toggle_);
    }

    std::string hold_string() const {
      return hold_.IsEmpty() ? std::string()
                             : ui::Command::AcceleratorToString(hold_);
    }

    MultiStep SendToggle() {
      return test_->Steps(
          test_->SendAccelerator(kBrowserViewElementId, toggle_),
          test_->Do([this] {
            // When toggle and press-and-hold share the same shortcut, on Aura
            // (Linux/Win/CrOS) `SendAccelerator` invokes
            // `FocusManager::ProcessAccelerator` without synthesizing a native
            // KeyUp event, so dispatch the release event if the hold watcher is
            // waiting for it.
            if (HotkeyHoldWatcher* watcher = test_->hold_watcher()) {
              ui::KeyEvent release_event(ui::EventType::kKeyReleased,
                                         toggle_.key_code(),
                                         toggle_.modifiers());
              watcher->OnEvent(release_event);
            }
          }));
    }

    MultiStep StartHold() {
      return test_->Steps(test_->Do([this] {
        auto* service = DictationKeyedService::Get(test_->profile());
        ASSERT_NE(service->hotkey_manager_for_testing(), nullptr);
        EXPECT_TRUE(
            service->hotkey_manager_for_testing()->AcceleratorPressed(hold_));
      }));
    }

    MultiStep EndHold() {
      return test_->Steps(test_->Do([this] {
        HotkeyHoldWatcher* watcher = test_->hold_watcher();
        ASSERT_NE(watcher, nullptr);
        ui::KeyEvent release_event(ui::EventType::kKeyReleased,
                                   hold_.key_code(), hold_.modifiers());
        watcher->OnEvent(release_event);
      }));
    }

   private:
    raw_ref<DictationHotkeysInteractiveUiTest> test_;
    ui::Accelerator toggle_;
    ui::Accelerator hold_;
  };

  HotkeyHoldWatcher* hold_watcher() {
    HotkeyManager* manager = dictation_service().hotkey_manager_for_testing();
    return manager ? manager->hold_watcher_for_testing() : nullptr;
  }

  // Waits for `duration` while allowing timer tasks to run.
  StepBuilder Sleep(base::TimeDelta duration) {
    return Do([duration] {
      // Have to allow nestable tasks to use this within a `RunTestSequence()`
      // call.
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), duration);
      run_loop.Run();
    });
  }

  void NavigateAndConfigureHotkeys(const GURL& url,
                                   std::string_view toggle_hotkey,
                                   std::string_view press_and_hold_hotkey) {
    // Hotkeys are only enabled once onboarding completes.
    profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                      true);
    profile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey, toggle_hotkey);
    profile()->GetPrefs()->SetString(prefs::kVoiceTypingPressAndHoldHotkey,
                                     press_and_hold_hotkey);
    DictationKeyedService::Get(profile())->DidInstallConnector();

    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
    content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
    content::MainThreadFrameObserver frame_observer(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
    frame_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList press_and_hold_feature_{kDictationPressAndHold};
};

IN_PROC_BROWSER_TEST_F(DictationHotkeysInteractiveUiTest,
                       HotkeyStartEndSession) {
  HotkeySimulator hotkeys(
      *this,
      /*toggle=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN),
      /*hold=*/ui::Accelerator());
  NavigateAndConfigureHotkeys(
      embedded_test_server()->GetURL("/textinput/simple_textarea.html"),
      hotkeys.toggle_string(), hotkeys.hold_string());

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this] {
      FocusTextarea();

      auto* focused_frame = web_contents()->GetFocusedFrame();
      ASSERT_NE(focused_frame, nullptr);
      EXPECT_EQ(focused_frame->GetFocusedEditableLevel(),
                content::EditableLevel::kPlaintextEditable);

      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service, nullptr);
      ASSERT_TRUE(service->IsEnabledAndReady());
      ASSERT_EQ(service->session_controller(), nullptr);

      // Verify the browser under test. GetLastActiveBrowser is preferred over
      // GetActiveBrowser here to avoid test flakiness due to async focus
      // propagation.
      auto* active_browser =
          GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
      ASSERT_NE(active_browser, nullptr);
      EXPECT_EQ(active_browser, browser());
    }),

    hotkeys.SendToggle(),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service->session_controller(), nullptr);
      // ToggleHotkeyHandler does not attach a HotkeyHoldWatcher.
      EXPECT_EQ(hold_watcher(), nullptr);
      auto* stream_provider =
          service->session_controller()->attached_stream_provider();
      ASSERT_NE(stream_provider, nullptr);
      auto* target = stream_provider->GetTarget();
      ASSERT_NE(target, nullptr);

      std::optional<int> expected_node_id = content::GetDOMNodeId(
          *web_contents()->GetPrimaryMainFrame(), "#text_id");
      ASSERT_TRUE(expected_node_id.has_value());
      EXPECT_EQ(target->global_dom_node_id().target_element_dom_id,
                blink::DOMNodeIdType(*expected_node_id));
    }),

    hotkeys.SendToggle(),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      EXPECT_NE(service->session_controller(), nullptr);
      EXPECT_EQ(service->session_controller()->attached_stream_provider(),
                nullptr);
    })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeysInteractiveUiTest,
                       PressAndHoldHotkeyStartAndReleaseEndsStream) {
  HotkeySimulator hotkeys(
      *this,
      /*toggle=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN),
      /*hold=*/
      ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
  NavigateAndConfigureHotkeys(
      embedded_test_server()->GetURL("/textinput/simple_textarea.html"),
      hotkeys.toggle_string(), hotkeys.hold_string());

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this] {
      FocusTextarea();
    }),

    hotkeys.StartHold(),
    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service->session_controller(), nullptr);
      ASSERT_NE(service->session_controller()->attached_stream_provider(),
                nullptr);
      ASSERT_NE(hold_watcher(), nullptr);
      EXPECT_EQ(
          hold_watcher()->hold_threshold_for_testing(),
          kDictationPressAndHoldThreshold.Get());
      EXPECT_FALSE(
          hold_watcher()->is_press_and_hold_mode());
    }),

    // Holding past `kDictationPressAndHoldThreshold` switches to
    // press-and-hold mode, and releasing the key ends the stream.
    Sleep(kDictationPressAndHoldThreshold.Get()),
    Do([this] {
      ASSERT_NE(hold_watcher(), nullptr);
      EXPECT_TRUE(hold_watcher()->is_press_and_hold_mode());
    }),
    hotkeys.EndHold(),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      EXPECT_EQ(hold_watcher(), nullptr);
      EXPECT_EQ(service->session_controller()->attached_stream_provider(),
                nullptr);
    })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeysInteractiveUiTest,
                       SharedHotkeyQuickTapStaysInToggleMode) {
  // Configure both toggle and press-and-hold to the SAME shortcut.
  HotkeySimulator hotkeys(
      *this,
      /*toggle=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN),
      /*hold=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN));
  NavigateAndConfigureHotkeys(
      embedded_test_server()->GetURL("/textinput/simple_textarea.html"),
      hotkeys.toggle_string(), hotkeys.hold_string());

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this] {
      FocusTextarea();
    }),

    // Quick tap (< `kDictationSharedHotkeyPressAndHoldThreshold`):
    // `HotkeyHoldWatcher` observes KeyUp before the threshold and keeps the
    // stream active in toggle mode.
    hotkeys.SendToggle(),
    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service->session_controller(), nullptr);
      EXPECT_EQ(hold_watcher(), nullptr);
      EXPECT_NE(service->session_controller()->attached_stream_provider(),
                nullptr);
    }),

    // Second hotkey press toggles the stream off.
    hotkeys.SendToggle(),
    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      EXPECT_EQ(service->session_controller()->attached_stream_provider(),
                nullptr);
    })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationHotkeysInteractiveUiTest,
                       SharedHotkeyHoldPastThresholdEndsOnRelease) {
  // Configure both toggle and press-and-hold to the SAME shortcut.
  HotkeySimulator hotkeys(
      *this,
      /*toggle=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN),
      /*hold=*/ui::Accelerator(ui::VKEY_D, ui::EF_ALT_DOWN));
  NavigateAndConfigureHotkeys(
      embedded_test_server()->GetURL("/textinput/simple_textarea.html"),
      hotkeys.toggle_string(), hotkeys.hold_string());

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this] {
      FocusTextarea();
    }),

    // Hold PAST `kDictationSharedHotkeyPressAndHoldThreshold`: switches to
    // press-and-hold mode and stops when the key or modifier is released.
    hotkeys.StartHold(),
    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service->session_controller(), nullptr);
      ASSERT_NE(service->session_controller()->attached_stream_provider(),
                nullptr);
      ASSERT_NE(hold_watcher(), nullptr);
      EXPECT_EQ(
          hold_watcher()->hold_threshold_for_testing(),
          kDictationSharedHotkeyPressAndHoldThreshold.Get());
    }),

    Sleep(kDictationSharedHotkeyPressAndHoldThreshold.Get()),
    Do([this] {
      ASSERT_NE(hold_watcher(), nullptr);
      EXPECT_TRUE(hold_watcher()->is_press_and_hold_mode());
    }),
    hotkeys.EndHold(),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      EXPECT_EQ(hold_watcher(), nullptr);
      EXPECT_EQ(service->session_controller()->attached_stream_provider(),
                nullptr);
    })
  );
  // clang-format on
}

}  // namespace dictation
