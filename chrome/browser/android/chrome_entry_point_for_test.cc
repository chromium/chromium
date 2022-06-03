// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/test/test_support_android.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/browser/android/chrome_jni_for_test_registration.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/network_service_test_helper.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

content::NetworkServiceTestHelper* GetNetworkServiceTestHelper() {
  static base::NoDestructor<content::NetworkServiceTestHelper> instance;
  return instance.get();
}

bool NativeInit(base::android::LibraryProcessType) {
  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess &&
      command_line->GetSwitchValueASCII(switches::kUtilitySubType) ==
          network::mojom::NetworkService::Name_) {
    ChromeContentUtilityClient::SetNetworkBinderCreationCallback(base::BindOnce(
        [](content::NetworkServiceTestHelper* helper,
           service_manager::BinderRegistry* registry) {
          helper->RegisterNetworkBinders(registry);
        },
        GetNetworkServiceTestHelper()));
  }

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
