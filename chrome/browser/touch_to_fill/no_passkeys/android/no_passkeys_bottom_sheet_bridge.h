// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_NO_PASSKEYS_ANDROID_NO_PASSKEYS_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_NO_PASSKEYS_ANDROID_NO_PASSKEYS_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/types/pass_key.h"

namespace ui {
class WindowAndroid;
}

class NoPasskeysBottomSheetBridge {
 public:
  // This class allows to mock/fake the actual JNI calls. The implementation
  // should perform no work other than JNI calls. No logic, no conditions.
  class JniDelegate {
   public:
    JniDelegate();
    JniDelegate(const JniDelegate&) = delete;
    JniDelegate& operator=(const JniDelegate&) = delete;
    virtual ~JniDelegate() = 0;

    // Creates a new java object.
    virtual void Create(ui::WindowAndroid* window_android) = 0;

    // Triggers the JNI call to show the sheet.
    virtual void Show(const std::string& origin) = 0;

    // Triggers the JNI call to dismiss and clean up the sheet.
    virtual void Dismiss() = 0;
  };

  NoPasskeysBottomSheetBridge();
  NoPasskeysBottomSheetBridge(
      base::PassKey<class NoPasskeysBottomSheetBridgeTest>,
      std::unique_ptr<JniDelegate> jni_delegate);
  NoPasskeysBottomSheetBridge(const NoPasskeysBottomSheetBridge&) = delete;
  NoPasskeysBottomSheetBridge& operator=(const NoPasskeysBottomSheetBridge&) =
      delete;
  ~NoPasskeysBottomSheetBridge();

  // Shows the bottom sheet and calls `on_dismissed_callback` once it's gone.
  void Show(ui::WindowAndroid* window_android,
            const std::string& origin,
            base::OnceClosure on_dismissed_callback);

  // Hides the sheet. This should result in an OnDismissed() call.
  void Dismiss();

  // Called via JNI when the sheet is dismissed.
  void OnDismissed(JNIEnv* env);

 private:
  // Forwards all requests to JNI. Can be replaced in tests.
  std::unique_ptr<JniDelegate> jni_delegate_;

  // The owner of this bridge sets this callback to clean the bridge up.
  base::OnceClosure on_dismissed_callback_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_NO_PASSKEYS_ANDROID_NO_PASSKEYS_BOTTOM_SHEET_BRIDGE_H_
