// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_
#define CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_

#include "base/functional/callback.h"

namespace ui {
class WindowAndroid;
}

namespace glic {

// Shows a snackbar indicating that microphone permission is disabled.
void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android);

// Shows a dialog asking the user to grant microphone permission for Gemini.
// The callback is invoked exactly once: with true if the user clicks "Allow",
// or false if the user clicks "No thanks" or dismisses the dialog.
// The dialog is owned by Java and can outlive the caller (it is dismissed, and
// therefore the callback is run, when the activity goes away). Callers must
// bind `callback` to a base::WeakPtr (or otherwise guarantee the bound state
// outlives the dialog) to avoid use-after-free.
void ShowMicPermissionDialog(ui::WindowAndroid* window_android,
                             base::OnceCallback<void(bool)> callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_
