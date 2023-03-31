// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/functional/bind.h"
#include "base/test/test_support_android.h"
#include "chrome/android/chrome_jni_for_test_registration_generated.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/test/network_service_test_helper.h"

namespace {

bool NativeInit(base::android::LibraryProcessType) {
  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  static std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper = content::NetworkServiceTestHelper::Create();

  return android::OnJNIOnLoadInit();
}

void RegisterNonMainDexNatives() {
  RegisterNonMainDexNatives(base::android::AttachCurrentThread());
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  // All MainDex JNI methods are registered. Since render processes don't need
  // very much Java code, we enable selective JNI registration on the
  // Java side and only register Non-MainDex JNI when necessary through
  // RegisterNonMainDexNatives().
  base::android::InitVM(vm);
  if (!RegisterMainDexNatives(base::android::AttachCurrentThread())) {
    return -1;
  }
  base::android::SetNonMainDexJniRegistrationHook(RegisterNonMainDexNatives);
  base::android::SetNativeInitializationHook(NativeInit);
  return JNI_VERSION_1_4;
}
