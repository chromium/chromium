// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_element_identifiers.h"
#include "chrome/browser/ash/accessibility/chromevox_panel.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

// Integration tests for the ChromeVox screen reader. These tests run on
// physical devices and VMs running a complete ChromeOS image.
class SpokenFeedbackIntegrationTest : public AshIntegrationTest {
 public:
  SpokenFeedbackIntegrationTest() = default;
  ~SpokenFeedbackIntegrationTest() override = default;
  SpokenFeedbackIntegrationTest(const SpokenFeedbackIntegrationTest&) = delete;
  SpokenFeedbackIntegrationTest& operator=(
      const SpokenFeedbackIntegrationTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(SpokenFeedbackIntegrationTest, KeyboardShortcut) {
  SetupContextWidget();
  RunTestSequence(Log("Enabling ChromeVox with the keyboard shortcut"), Do([] {
                    ui_controls::SendKeyPress(/*window=*/nullptr, ui::VKEY_Z,
                                              /*control=*/true, /*shift=*/false,
                                              /*alt=*/true, /*command=*/false);
                  }),
                  Log("Waiting for the ChromeVox panel to show"),
                  WaitForShow(kChromeVoxPanelElementId));
}

}  // namespace ash
