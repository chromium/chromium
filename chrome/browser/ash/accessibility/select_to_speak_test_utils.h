// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

class Profile;

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

// Utilities for exercising Select to Speak in browsertests.
namespace ash {
class AutomationTestUtils;

namespace sts_test_utils {

// Turns on Select to Speak and waits for the extension to signal it is ready.
// Disables enhanced network voices dialog so that it will not block UI.
void TurnOnSelectToSpeakForTest(Profile* profile);

// Hold down Search and drag over the web contents to select everything.
void StartSelectToSpeakInBrowserWithUrl(const std::string& url,
                                        AutomationTestUtils* test_utils,
                                        ui::test::EventGenerator* generator);

// Hold down Search and drag over the bounds to select everything.
void StartSelectToSpeakWithBounds(const gfx::Rect& bounds,
                                  ui::test::EventGenerator* generator);

}  // namespace sts_test_utils
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SELECT_TO_SPEAK_TEST_UTILS_H_
