// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_

class Browser;

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

// Utilities for exercising Select to Speak in browsertests.
namespace ash::sts_test_utils {

// Turns on Select to Speak and waits for the extension to signal it is ready.
// Disables enhanced network voices dialog so that it will not block UI.
void TurnOnSelectToSpeakForTest(Browser* browser);

// Hold down Search and drag over the web contents to select everything.
void StartSelectToSpeakInBrowserWindow(Browser* browser,
                                       ui::test::EventGenerator* generator);

}  // namespace ash::sts_test_utils

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_
