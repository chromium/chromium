// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_

#include <jni.h>
#include <memory>
#include <queue>

#include "base/android/jni_android.h"
#include "chrome/browser/android/tab_android.h"

namespace vr {

// Installs the VR module.
class VrModuleProvider {
 public:
  explicit VrModuleProvider(TabAndroid* tab);
  ~VrModuleProvider();

  bool ModuleInstalled();
  void InstallModule(base::OnceCallback<void(bool)> on_finished);

  // Called by Java.
  void OnInstalledModule(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         bool success);

 private:
  std::queue<base::OnceCallback<void(bool)>> on_finished_callbacks_;
  base::android::ScopedJavaGlobalRef<jobject> j_vr_module_provider_;
  TabAndroid* tab_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Creates a VR module provider.
class VrModuleProviderFactory {
 public:
  static std::unique_ptr<VrModuleProvider> CreateModuleProvider(
      int render_process_id,
      int render_frame_id);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_MODULE_PROVIDER_H_
