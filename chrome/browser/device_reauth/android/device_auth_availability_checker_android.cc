// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_auth_availability_checker_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/device_reauth/android/jni_headers/DeviceAuthAvailabilityCheckerBridge_jni.h"

using base::android::AttachCurrentThread;
using device_reauth::BiometricsAvailability;
using device_reauth::BiometricStatus;

namespace {

class DeviceAuthAvailabilityCheckerBridgeImpl
    : public DeviceAuthAvailabilityCheckerAndroid::Bridge {
 public:
  DeviceAuthAvailabilityCheckerBridgeImpl() = default;
  ~DeviceAuthAvailabilityCheckerBridgeImpl() override = default;

  BiometricsAvailability CanAuthenticateWithBiometric() override {
    return Java_DeviceAuthAvailabilityCheckerBridge_canAuthenticateWithBiometric(
        AttachCurrentThread());
  }

  bool CanAuthenticateWithBiometricOrScreenLock() override {
    return Java_DeviceAuthAvailabilityCheckerBridge_canAuthenticateWithBiometricOrScreenLock(
        AttachCurrentThread());
  }
};

}  // namespace

DeviceAuthAvailabilityCheckerAndroid::DeviceAuthAvailabilityCheckerAndroid()
    : DeviceAuthAvailabilityCheckerAndroid(
          std::make_unique<DeviceAuthAvailabilityCheckerBridgeImpl>()) {}

DeviceAuthAvailabilityCheckerAndroid::DeviceAuthAvailabilityCheckerAndroid(
    std::unique_ptr<Bridge> bridge)
    : bridge_(std::move(bridge)) {}

DeviceAuthAvailabilityCheckerAndroid::~DeviceAuthAvailabilityCheckerAndroid() =
    default;

bool DeviceAuthAvailabilityCheckerAndroid::CanAuthenticateWithBiometrics() {
  BiometricsAvailability availability = bridge_->CanAuthenticateWithBiometric();
  return availability == BiometricsAvailability::kAvailable;
}

bool DeviceAuthAvailabilityCheckerAndroid::
    CanAuthenticateWithBiometricOrScreenLock() {
  return bridge_->CanAuthenticateWithBiometricOrScreenLock();
}

BiometricStatus
DeviceAuthAvailabilityCheckerAndroid::GetBiometricAvailabilityStatus() {
  BiometricsAvailability availability = bridge_->CanAuthenticateWithBiometric();
  switch (availability) {
    case BiometricsAvailability::kAvailable:
      return BiometricStatus::kBiometricsAvailable;
    // TODO (crbug.com/369057610): Probably return status `kAvailable` for
    // `BiometricsAvailability::kAvailableNoFallback` case.
    case BiometricsAvailability::kAvailableNoFallback:
    case BiometricsAvailability::kNoHardware:
    case BiometricsAvailability::kHwUnavailable:
    case BiometricsAvailability::kNotEnrolled:
    case BiometricsAvailability::kSecurityUpdateRequired:
    case BiometricsAvailability::kOtherError:
      break;
  }
  // TODO (crbug.com/368586157): Call just hasScreenLockSetUp here.
  if (CanAuthenticateWithBiometricOrScreenLock()) {
    return BiometricStatus::kOnlyLskfAvailable;
  }
  return BiometricStatus::kUnavailable;
}

DEFINE_JNI(DeviceAuthAvailabilityCheckerBridge)
