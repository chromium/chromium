// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace infobars {
class InfoBarManager;
}

namespace vr {
// Equivalent of ArCoreApk.Availability enum.
// For detailed description, please see:
// https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk.Availability
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class ArCoreAvailability : int {
  kSupportedApkTooOld = 0,
  kSupportedInstalled = 1,
  kSupportedNotInstalled = 2,
  kUnknownChecking = 3,
  kUnknownError = 4,
  kUnknownTimedOut = 5,
  kUnsupportedDeviceNotCapable = 6,
};

class ArCoreInstallHelper {
 public:
  ArCoreInstallHelper();
  virtual ~ArCoreInstallHelper();

  ArCoreInstallHelper(const ArCoreInstallHelper&) = delete;
  ArCoreInstallHelper& operator=(const ArCoreInstallHelper&) = delete;

  void EnsureInstalled(int render_process_id,
                       int render_frame_id,
                       infobars::InfoBarManager* infobar_manager,
                       base::OnceCallback<void(bool)> install_callback);

  // Called from Java end.
  void OnRequestInstallSupportedArCoreResult(JNIEnv* env, bool success);

 private:
  void ShowInfoBar(int render_process_id,
                   int render_frame_id,
                   infobars::InfoBarManager* infobar_manager);
  void OnInfoBarResponse(int render_process_id,
                         int render_frame_id,
                         bool try_install);
  void RunInstallFinishedCallback(bool succeeded);

  base::OnceCallback<void(bool)> install_finished_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;

  // Must be last.
  base::WeakPtrFactory<ArCoreInstallHelper> weak_ptr_factory_{this};
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_INSTALL_HELPER_H_
