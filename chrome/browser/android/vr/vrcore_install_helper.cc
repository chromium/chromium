// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vrcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/android/features/vr/split_jni_headers/VrCoreInstallUtils_jni.h"
#include "chrome/browser/android/vr/vr_module_provider.h"
#include "components/webxr/android/webxr_utils.h"

using base::android::AttachCurrentThread;

namespace vr {

VrCoreInstallHelper::VrCoreInstallHelper(
    const VrModuleProvider& vr_module_provider)
    : XrInstallHelper() {
  DVLOG(1) << __func__;

  DCHECK(vr_module_provider.ModuleInstalled());

  // Kick off a call to getVrSupportLevel, since it will cache the result after
  // the first query.
  Java_VrCoreInstallUtils_getVrSupportLevel(AttachCurrentThread());

  java_install_utils_ = Java_VrCoreInstallUtils_create(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));
}

VrCoreInstallHelper::~VrCoreInstallHelper() {
  DVLOG(1) << __func__;
  if (!java_install_utils_.is_null()) {
    Java_VrCoreInstallUtils_onNativeDestroy(AttachCurrentThread(),
                                            java_install_utils_);
  }

  RunInstallFinishedCallback(false);
}

/* static */ bool VrCoreInstallHelper::VrSupportNeedsUpdate() {
  return Java_VrCoreInstallUtils_vrSupportNeedsUpdate(AttachCurrentThread());
}

void VrCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  DVLOG(1) << __func__;
  DCHECK(!install_finished_callback_);
  install_finished_callback_ = std::move(install_callback);

  if (java_install_utils_.is_null()) {
    RunInstallFinishedCallback(false);
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  if (Java_VrCoreInstallUtils_vrSupportNeedsUpdate(env)) {
    // VrCore is not installed or requires an update.
    // When completed, java will call: OnInstallResult
    Java_VrCoreInstallUtils_requestInstallVrCore(
        env, java_install_utils_,
        webxr::GetJavaWebContents(render_process_id, render_frame_id));
    return;
  }

  // VrCore did not need to be installed/updated so mock out that its
  // installation succeeded.
  OnInstallResult(nullptr, true);
}

void VrCoreInstallHelper::OnInstallResult(JNIEnv* env, bool success) {
  // Nothing else to do, simply call the deferred callback.
  RunInstallFinishedCallback(success);
}

void VrCoreInstallHelper::RunInstallFinishedCallback(bool succeeded) {
  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(succeeded);
  }
}
}  // namespace vr
