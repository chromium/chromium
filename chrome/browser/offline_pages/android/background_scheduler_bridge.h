// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_BACKGROUND_SCHEDULER_BRIDGE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_BACKGROUND_SCHEDULER_BRIDGE_H_

#include <stdint.h>
#include <memory>

#include "base/android/jni_android.h"
#include "components/offline_pages/core/background/scheduler.h"

namespace offline_pages {
namespace android {

// Bridge between C++ and Java for implementing background scheduler
// on Android.
class BackgroundSchedulerBridge : public Scheduler {
 public:
  BackgroundSchedulerBridge();
  ~BackgroundSchedulerBridge() override;

  // Scheduler implementation.
  void Schedule(const TriggerConditions& trigger_conditions) override;
  void BackupSchedule(const TriggerConditions& trigger_conditions,
                      int64_t delay_in_seconds) override;
  void Unschedule() override;
  const DeviceConditions& GetCurrentDeviceConditions() override;

 private:
  base::android::ScopedJavaLocalRef<jobject> CreateTriggerConditions(
      JNIEnv* env,
      bool require_power_connected,
      int minimum_battery_percentage,
      bool require_unmetered_network) const;
  std::unique_ptr<DeviceConditions> device_conditions_;
};

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_BACKGROUND_SCHEDULER_BRIDGE_H_
