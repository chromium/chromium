// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_MODE_H_
#define CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_MODE_H_

namespace chrome {
namespace android {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.feedback
enum ScreenshotMode {
  // The screenshot mode will be decided based on the activity's state.
  DEFAULT = 0,
  // Takes a compositor screenshot.
  COMPOSITOR = 1,
  // Takes a screenshot of the Android view.
  ANDROID_VIEW = 2
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_FEEDBACK_SCREENSHOT_MODE_H_
