// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_

#include "ash/public/cpp/accelerators.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace ui::test {
class EventGenerator;
}

namespace ash {

using ::extensions::api::braille_display_private::StubBrailleController;

// Spoken feedback tests in both a logged in browser window and guest mode.
enum SpokenFeedbackTestVariant { kTestAsNormalUser, kTestAsGuestUser };

// A class used to define the parameters of a spoken feedback test case.
class SpokenFeedbackTestConfig {
 public:
  explicit SpokenFeedbackTestConfig(ManifestVersion manifest_version)
      : manifest_version_(manifest_version) {}

  SpokenFeedbackTestConfig(ManifestVersion manifest_version,
                           SpokenFeedbackTestVariant variant)
      : manifest_version_(manifest_version), variant_(variant) {}

  SpokenFeedbackTestConfig(ManifestVersion manifest_version,
                           SpokenFeedbackTestVariant variant,
                           bool tablet_mode)
      : manifest_version_(manifest_version),
        variant_(variant),
        tablet_mode_(tablet_mode) {}

  ManifestVersion manifest_version() const { return manifest_version_; }
  std::optional<SpokenFeedbackTestVariant> variant() const { return variant_; }
  std::optional<bool> tablet_mode() const { return tablet_mode_; }

 private:
  ManifestVersion manifest_version_;
  std::optional<SpokenFeedbackTestVariant> variant_;
  std::optional<bool> tablet_mode_;
};

// Spoken feedback tests only in a logged in user's window.
class LoggedInSpokenFeedbackTest
    : public AccessibilityFeatureBrowserTest,
      public ::testing::WithParamInterface<SpokenFeedbackTestConfig> {
 public:
  LoggedInSpokenFeedbackTest();

  LoggedInSpokenFeedbackTest(const LoggedInSpokenFeedbackTest&) = delete;
  LoggedInSpokenFeedbackTest& operator=(const LoggedInSpokenFeedbackTest&) =
      delete;

  ~LoggedInSpokenFeedbackTest() override;

  // AccessibilityFeatureBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Simulate key press event.
  void SendKeyPress(ui::KeyboardCode key);
  void SendKeyPressWithControl(ui::KeyboardCode key);
  void SendKeyPressWithControlAndAlt(ui::KeyboardCode key);
  void SendKeyPressWithControlAndShift(ui::KeyboardCode key);
  void SendKeyPressWithShift(ui::KeyboardCode key);
  void SendKeyPressWithAltAndShift(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndShift(ui::KeyboardCode key);
  void SendKeyPressWithSearch(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndControl(ui::KeyboardCode key);
  void SendKeyPressWithSearchAndControlAndShift(ui::KeyboardCode key);

  void SendStickyKeyCommand();

  void SendMouseMoveTo(const gfx::Point& location);
  void SetMouseSourceDeviceId(int id);

  bool PerformAcceleratorAction(AcceleratorAction action);

  void StablizeChromeVoxState();

  void PressRepeatedlyUntilUtterance(ui::KeyboardCode key,
                                     const std::string& expected_utterance);

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

  ChromeVoxTestUtils* chromevox_test_utils() {
    return chromevox_test_utils_.get();
  }

  test::SpeechMonitor* sm() { return chromevox_test_utils()->sm(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  std::unique_ptr<ChromeVoxTestUtils> chromevox_test_utils_;

  StubBrailleController braille_controller_;
  gfx::ScopedAnimationDurationScaleMode animation_mode_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SPOKEN_FEEDBACK_BROWSERTEST_H_
