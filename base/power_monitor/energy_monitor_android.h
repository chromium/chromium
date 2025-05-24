// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_ENERGY_MONITOR_ANDROID_H_
#define BASE_POWER_MONITOR_ENERGY_MONITOR_ANDROID_H_

#include <jni.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "build/build_config.h"

namespace base {
namespace android {

// Read and return the current remaining battery capacity (microampere-hours).
// Only supported with a device power source (i.e. not in child processes in
// Chrome) and on devices with Android >= Lollipop as well as a power supply
// that supports this counter. Returns 0 if unsupported.
int BASE_EXPORT GetRemainingBatteryCapacity();

// Total energy consumed in microwatt-seconds for a subsystem. The exact list of
// consumers and the meaning of each consumer depends on the device
// https://developer.android.com/reference/android/os/PowerMonitor#POWER_MONITOR_TYPE_CONSUMER.
struct PowerMonitorReading {
  std::string consumer;
  int64_t total_energy;
};

// Java -> Native conversion function.
PowerMonitorReading FromJavaPowerMonitorReading(
    JNIEnv* env,
    const JavaRef<jobject>& jobject);

// Read and return the total energy consumed per subsystem since boot in
// microwatt-seconds. Only supported on specific devices with Android >= Vanilla
// Ice Cream. Returns an empty vector if unsupported. This should be called only
// after we know the battery status from
// `PowerMonitor::AddPowerStateObserverAndReturnBatteryPowerStatus`. Otherwise
// the monitor might be not initialized, and this function may return an empty
// vector.
std::vector<PowerMonitorReading> BASE_EXPORT GetTotalEnergyConsumed();

}  // namespace android
}  // namespace base

namespace jni_zero {

template <>
inline base::android::PowerMonitorReading
FromJniType<base::android::PowerMonitorReading>(
    JNIEnv* env,
    const JavaRef<jobject>& jobject) {
  return base::android::FromJavaPowerMonitorReading(env, jobject);
}

}  // namespace jni_zero

#endif  // BASE_POWER_MONITOR_ENERGY_MONITOR_ANDROID_H_
