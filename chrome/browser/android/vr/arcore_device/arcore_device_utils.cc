// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/android/vr/ar_jni_headers/ArCompositorDelegateProviderImpl_jni.h"
#include "chrome/browser/android/vr/ar_jni_headers/ArCoreDeviceUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
#include "components/webxr/android/ar_compositor_delegate_provider.h"
#include "device/vr/android/arcore/arcore_device_provider_factory.h"

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
  base::android::ScopedJavaLocalRef<jobject> j_ar_compositor_delegate_provider =
      vr::Java_ArCompositorDelegateProviderImpl_Constructor(
          base::android::AttachCurrentThread());

  return std::make_unique<device::ArCoreDeviceProvider>(
      webxr::ArCompositorDelegateProvider(
          std::move(j_ar_compositor_delegate_provider)));
}

}  // namespace

static void JNI_ArCoreDeviceUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
