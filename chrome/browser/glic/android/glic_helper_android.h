// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_

namespace ui {
class WindowAndroid;
}

namespace glic {

// Shows a snackbar indicating that microphone permission is disabled.
void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_
