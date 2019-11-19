// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/android/chrome_jni_headers/DecoderService_jni.h"
#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"
#include "sandbox/sandbox_buildflags.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

void JNI_DecoderService_InitializePhotoPickerSandbox(JNIEnv* env) {
  auto* info = base::android::BuildInfo::GetInstance();
  sandbox::SeccompStarterAndroid starter(info->sdk_int(), info->device());

#if BUILDFLAG(USE_SECCOMP_BPF)
  // The policy compiler is only available if USE_SECCOMP_BPF is enabled.
  starter.set_policy(std::make_unique<sandbox::BaselinePolicyAndroid>());
#endif
  starter.StartSandbox();

  UMA_HISTOGRAM_ENUMERATION("Android.SeccompStatus.PhotoPickerSandbox",
                            starter.status(),
                            sandbox::SeccompSandboxStatus::STATUS_MAX);
}
