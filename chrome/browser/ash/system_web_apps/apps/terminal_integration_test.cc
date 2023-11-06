// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_pref_names.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/aura/env.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"

namespace {

class TerminalIntegrationTest : public InteractiveAshTest {
 public:
  // Sends the given text to the element as individual key press commands.
  //
  // This handles uppercase and lowercase ASCII letters and numbers, plus maps
  // "\n" to return, but no symbols or other shifted things.
  //
  // TODO(crbug.com/1495154) have a more supported way to do this and remove
  // this function.
  auto SendTextAsKeyEvents(const ui::ElementIdentifier& element_id,
                           const std::string& text) {
    MultiStep steps;
    for (char c : text) {
      if (c >= 'a' && c <= 'z') {
        AddStep(steps,
                SendAccelerator(
                    element_id,
                    ui::Accelerator(
                        static_cast<ui::KeyboardCode>(
                            static_cast<unsigned char>(ui::VKEY_A) + (c - 'a')),
                        0, ui::Accelerator::KeyState::PRESSED)));
      } else if (c >= 'A' && c <= 'Z') {
        AddStep(
            steps,
            SendAccelerator(
                element_id,
                ui::Accelerator(
                    static_cast<ui::KeyboardCode>(
                        static_cast<unsigned char>(ui::VKEY_A) + (c - 'A')),
                    ui::EF_SHIFT_DOWN, ui::Accelerator::KeyState::PRESSED)));
      } else if (c >= '0' && c <= '9') {
        AddStep(steps,
                SendAccelerator(
                    element_id,
                    ui::Accelerator(
                        static_cast<ui::KeyboardCode>(
                            static_cast<unsigned char>(ui::VKEY_0) + (c - '0')),
                        0, ui::Accelerator::KeyState::PRESSED)));
      } else if (c == '\n') {
        AddStep(steps,
                SendAccelerator(
                    element_id,
                    ui::Accelerator(ui::VKEY_RETURN, 0,
                                    ui::Accelerator::KeyState::PRESSED)));
      } else {
        // Unsupported input.
        NOTREACHED();
      }
    }
    return steps;
  }
};

IN_PROC_BROWSER_TEST_F(TerminalIntegrationTest, Crosh) {
  ui::ScopedAnimationDurationScaleMode zero_duration(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  // The terminal uses a <canvas> for output so everything is opaque to us.
  // Enabling accessibility puts it in a mode where it also outputs a DOM with
  // the text in it for screen readers, allowing us to inspect its output.
  auto* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  PrefService* prefs = profile->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(ash::prefs::kAccessibilitySpokenFeedbackEnabled, true);

  aura::Env* env = aura::Env::GetInstance();
  ASSERT_TRUE(env);

  SetupContextWidget();

  // Cros is included in the system web apps.
  InstallSystemApps();

  // The contents of the terminal window is inside an element with this ID.
  WebContentsInteractionTestUtil::DeepQuery terminal_node{"#terminal"};

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCroshWebContentsId);
  // clang-format off
  RunTestSequence(
      InstrumentNextTab(kCroshWebContentsId, AnyBrowser()),

      // Launch via the Control-Alt-T accelerator (arbitrarily sent to the
      // "home" button because Kombucha needs a target).
      Log("Launching Crosh"),
      SendAccelerator(
          ash::kHomeButtonElementId,
          ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN,
                          ui::Accelerator::KeyState::PRESSED)),
      WaitForWindowWithTitle(env, u"crosh"),
      WaitForWebContentsReady(kCroshWebContentsId),

      Log("Looking for crosh prompt"),
      WaitForElementTextContains(kCroshWebContentsId, terminal_node, "crosh>"),

      Log("Running the shell"),
      SendTextAsKeyEvents(kCroshWebContentsId, "shell\n"),

      // Note that the prompt and path will vary by board and build directory so
      // this does a very generic query.
      Log("Looking for shell prompt"),
      WaitForElementTextContains(kCroshWebContentsId, terminal_node,
                                 "chronos@"),

      Log("Exiting the shell"),
      SendTextAsKeyEvents(kCroshWebContentsId, "exit\n"),

      Log("Exiting the crosh prompt"),
      SendTextAsKeyEvents(kCroshWebContentsId, "exit\n"),

      Log("Waiting for exit"),
      WaitForHide(kCroshWebContentsId));
  // clang-format on
}

}  // namespace
