// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

// BackgroundSyncLauncherAndroid is used to register interest in starting
// the browser the next time the device goes online. This class runs on the UI
// thread.
class BackgroundSyncLauncherAndroid {
 public:
  static BackgroundSyncLauncherAndroid* Get();

  BackgroundSyncLauncherAndroid(const BackgroundSyncLauncherAndroid&) = delete;
  BackgroundSyncLauncherAndroid& operator=(
      const BackgroundSyncLauncherAndroid&) = delete;

  // Schedules a BackgroundTaskScheduler task for |sync_type| with delay |delay|
  // to ensure that the browser is running when the device next goes online
  // after that time has passed. If |delay| is base::TimeDelta::Max(), the
  // wake-up task is cancelled.
  static void ScheduleBrowserWakeUpWithDelay(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta delay);

  // Cancels the BackgroundTaskScheduler task that wakes up the browser to
  // process Background Sync registrations of type |sync_type|.
  static void CancelBrowserWakeup(blink::mojom::BackgroundSyncType sync_type);

  static bool ShouldDisableBackgroundSync();

  // TODO(crbug.com/40428648): Remove this once the bots have their play
  // services package updated before every test run.
  static void SetPlayServicesVersionCheckDisabledForTests(bool disabled);

  // Fires all pending Background Sync events across all storage partitions
  // for the last used profile.
  // Fires one-shot Background Sync events for registration of |sync_type|.
  void FireBackgroundSyncEvents(
      blink::mojom::BackgroundSyncType sync_type,
      const base::android::JavaParamRef<jobject>& j_runnable);

 private:
  friend struct base::LazyInstanceTraitsBase<BackgroundSyncLauncherAndroid>;

  // Constructor and destructor marked private to enforce singleton
  BackgroundSyncLauncherAndroid();
  ~BackgroundSyncLauncherAndroid();

  void ScheduleBrowserWakeUpWithDelayImpl(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta soonest_wakeup_delta);
  void CancelBrowserWakeupImpl(blink::mojom::BackgroundSyncType sync_type);

  base::android::ScopedJavaGlobalRef<jobject>
      java_background_sync_background_task_scheduler_launcher_;
};

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
