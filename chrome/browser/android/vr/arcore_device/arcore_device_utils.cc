// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/android/vr/ar_jni_headers/ArCoreDeviceUtils_jni.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"
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
  return std::make_unique<device::ArCoreDeviceProvider>();
}

}  // namespace

static void JNI_ArCoreDeviceUtils_InstallArCoreDeviceProviderFactory(
    JNIEnv* env) {
  device::ArCoreDeviceProviderFactory::Install(
      std::make_unique<ArCoreDeviceProviderFactoryImpl>());
}

}  // namespace vr
