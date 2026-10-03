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

// Android microphone permission UI for Glic. Owned by GlicKeyedService, and
// can be overridden in tests. Callbacks may run after the caller is gone, so
// bind them to a WeakPtr.
class MicPermissionUi {
 public:
  MicPermissionUi() = default;
  MicPermissionUi(const MicPermissionUi&) = delete;
  MicPermissionUi& operator=(const MicPermissionUi&) = delete;
  virtual ~MicPermissionUi() = default;

  // Shows Chrome's dialog asking to let Gemini use the microphone. Runs
  // `callback` with whether the user allowed it.
  virtual void ShowMicPermissionDialog(ui::WindowAndroid* window_android,
                                       base::OnceCallback<void(bool)> callback);

  // Returns whether the Android RECORD_AUDIO permission is granted.
  virtual bool HasMicOsPermission(ui::WindowAndroid* window_android);

  // Requests the Android RECORD_AUDIO permission. Runs `callback` with whether
  // it was granted.
  virtual void RequestMicOsPermission(ui::WindowAndroid* window_android,
                                      base::OnceCallback<void(bool)> callback);

  // Shows a snackbar saying the microphone permission is disabled.
  virtual void ShowMicDisabledSnackbar(ui::WindowAndroid* window_android);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ANDROID_GLIC_HELPER_ANDROID_H_
