// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/power_monitor/power_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/PowerMonitor_jni.h"

namespace base {

// A helper function which is a friend of PowerMonitorSource.
void ProcessPowerEventHelper(PowerMonitorSource::PowerEvent event) {
  PowerMonitorSource::ProcessPowerEvent(event);
}

// A helper function which is a friend of PowerMonitorSource.
void ProcessThermalEventHelper(
    PowerThermalObserver::DeviceThermalState new_thermal_state) {
  PowerMonitorSource::ProcessThermalEvent(new_thermal_state);
}

namespace android {

namespace {
using DeviceThermalState = PowerThermalObserver::DeviceThermalState;

// See
// https://developer.android.com/reference/android/os/PowerManager#THERMAL_STATUS_CRITICAL
enum AndroidThermalStatus {
  THERMAL_STATUS_NONE = 0,
  THERMAL_STATUS_LIGHT = 1,
  THERMAL_STATUS_MODERATE = 2,
  THERMAL_STATUS_SEVERE = 3,
  THERMAL_STATUS_CRITICAL = 4,
  THERMAL_STATUS_EMERGENCY = 5,
  THERMAL_STATUS_SHUTDOWN = 6,
};

PowerThermalObserver::DeviceThermalState MapToDeviceThermalState(
    int android_thermal_status) {
  switch (android_thermal_status) {
    case THERMAL_STATUS_NONE:
      return DeviceThermalState::kNominal;

    case THERMAL_STATUS_LIGHT:
    case THERMAL_STATUS_MODERATE:
      return DeviceThermalState::kFair;

    case THERMAL_STATUS_SEVERE:
      return DeviceThermalState::kSerious;

    case THERMAL_STATUS_CRITICAL:
    case THERMAL_STATUS_EMERGENCY:
    case THERMAL_STATUS_SHUTDOWN:
      return DeviceThermalState::kCritical;

    default:
      return DeviceThermalState::kUnknown;
  }
}

}  // namespace

// Native implementation of PowerMonitor.java. Note: This will be invoked by
// PowerMonitor.java shortly after startup to set the correct initial value for
// "is on battery power."
void JNI_PowerMonitor_OnBatteryChargingChanged(JNIEnv* env) {
  ProcessPowerEventHelper(PowerMonitorSource::POWER_STATE_EVENT);
}

void JNI_PowerMonitor_OnThermalStatusChanged(JNIEnv* env, int thermal_status) {
  ProcessThermalEventHelper(MapToDeviceThermalState(thermal_status));
}

// Note: Android does not have the concept of suspend / resume as it's known by
// other platforms. Thus we do not send Suspend/Resume notifications. See
// http://crbug.com/644515

}  // namespace android

PowerStateObserver::BatteryPowerStatus
PowerMonitorDeviceSource::GetBatteryPowerStatus() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  int battery_power =
      base::android::Java_PowerMonitor_getBatteryPowerStatus(env);
  return static_cast<PowerStateObserver::BatteryPowerStatus>(battery_power);
}

int PowerMonitorDeviceSource::GetRemainingBatteryCapacity() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return base::android::Java_PowerMonitor_getRemainingBatteryCapacity(env);
}

PowerThermalObserver::DeviceThermalState
PowerMonitorDeviceSource::GetCurrentThermalState() const {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  return android::MapToDeviceThermalState(
      android::Java_PowerMonitor_getCurrentThermalStatus(env));
}

}  // namespace base
