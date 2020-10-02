// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/android/vr/android_vr_utils.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreInstallUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"

using base::android::AttachCurrentThread;

namespace vr {

namespace {
class ArCoreDeviceProviderFactoryImpl
    : public device::ArCoreDeviceProviderFactory {
 public:
  ArCoreDeviceProviderFactoryImpl() = default;
  ~ArCoreDeviceProviderFactoryImpl() override = default;
  std::unique_ptr<device::VRDeviceProvider> CreateDeviceProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProviderFactoryImpl);
};

std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactoryImpl::CreateDeviceProvider() {
  return std::make_unique<device::ArCoreDeviceProvider>();
}

}  // namespace

ArCoreInstallHelper::ArCoreInstallHelper() : XrInstallHelper() {
  // As per documentation, it's recommended to issue a call to
  // ArCoreApk.checkAvailability() early in application lifecycle & ignore the
  // result so that subsequent calls can return cached result:
  // https://developers.google.com/ar/develop/java/enable-arcore
  // In the event that a remote call is required, it will not block on that
  // remote call per:
  // https://developers.google.com/ar/reference/java/arcore/reference/com/google/ar/core/ArCoreApk#checkAvailability
  Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(
      AttachCurrentThread());

  java_install_utils_ = Java_ArCoreInstallUtils_create(
      AttachCurrentThread(), reinterpret_cast<jlong>(this));
}

ArCoreInstallHelper::~ArCoreInstallHelper() {
  if (!java_install_utils_.is_null()) {
    Java_ArCoreInstallUtils_onNativeDestroy(AttachCurrentThread(),
                                            java_install_utils_);
  }

  RunInstallFinishedCallback(false);
}

void ArCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  DCHECK(!install_finished_callback_);
  install_finished_callback_ = std::move(install_callback);

  if (java_install_utils_.is_null()) {
    RunInstallFinishedCallback(false);
    return;
  }

  JNIEnv* env = AttachCurrentThread();

  if (Java_ArCoreInstallUtils_shouldRequestInstallSupportedArCore(env)) {
    // ARCore is not installed or requires an update.
    // When completed, java will call: OnRequestInstallSupportedArCoreResult
    Java_ArCoreInstallUtils_requestInstallSupportedArCore(
        env, java_install_utils_,
        GetJavaWebContents(render_process_id, render_frame_id));
    return;
  }

  // ARCore did not need to be installed/updated so mock out that its
  // installation succeeded.
  OnRequestInstallSupportedArCoreResult(nullptr, true);
}

void ArCoreInstallHelper::OnRequestInstallSupportedArCoreResult(JNIEnv* env,
                                                                bool success) {
  DVLOG(1) << __func__;

  // Nothing else to do, simply call the deferred callback.
  RunInstallFinishedCallback(success);
}

void ArCoreInstallHelper::RunInstallFinishedCallback(bool succeeded) {
  if (install_finished_callback_) {
    std::move(install_finished_callback_).Run(succeeded);
  }
}

static void JNI_ArCoreInstallUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
