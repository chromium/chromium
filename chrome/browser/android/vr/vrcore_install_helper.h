// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VRCORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_VRCORE_INSTALL_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/xr_install_helper.h"

namespace vr {

class VrModuleProvider;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class VrSupportLevel : int {
  kVrDisabled = 0,
  kVrNeedsUpdate = 1,  // VR Support is available, but needs update.
  kVrCardboard = 2,
  kVrDaydream = 3,  // Supports both Cardboard and Daydream viewer.
};

// Helper class for Installing VrCore. Note that this must not be created unless
// the VR DFM has been confirmed to be installed previously.
class VR_EXPORT VrCoreInstallHelper : public content::XrInstallHelper {
 public:
  explicit VrCoreInstallHelper(const VrModuleProvider& vr_module_provider);
  ~VrCoreInstallHelper() override;

  static bool VrSupportNeedsUpdate();

  VrCoreInstallHelper(const VrCoreInstallHelper&) = delete;
  VrCoreInstallHelper& operator=(const VrCoreInstallHelper&) = delete;

  void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> install_callback) override;

  // Called from Java end.
  void OnInstallResult(JNIEnv* env, bool success);

 private:
  void RunInstallFinishedCallback(bool succeeded);

  base::OnceCallback<void(bool)> install_finished_callback_;
  base::android::ScopedJavaGlobalRef<jobject> java_install_utils_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VRCORE_INSTALL_HELPER_H_
