// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_sync_launcher_android.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/BackgroundSyncBackgroundTaskScheduler_jni.h"
#include "chrome/android/chrome_jni_headers/BackgroundSyncBackgroundTask_jni.h"
#include "chrome/android/chrome_jni_headers/GooglePlayServicesChecker_jni.h"
#include "chrome/android/chrome_jni_headers/PeriodicBackgroundSyncChromeWakeUpTask_jni.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

using content::BrowserThread;

namespace {
base::LazyInstance<BackgroundSyncLauncherAndroid>::DestructorAtExit
    g_background_sync_launcher = LAZY_INSTANCE_INITIALIZER;

// Disables the Play Services version check for testing on Chromium build bots.
// TODO(iclelland): Remove this once the bots have their play services package
// updated before every test run. (https://crbug.com/514449)
bool disable_play_services_version_check_for_tests = false;

// Returns 0 to create a ONE_SHOT_SYNC_CHROME_WAKE_UP task, or 1 to create a
// PERIODIC_SYNC_CHROME_WAKE_UP task, based on |sync_type|.
int GetBackgroundTaskType(blink::mojom::BackgroundSyncType sync_type) {
  return static_cast<int>(sync_type);
}

}  // namespace

// static
void JNI_BackgroundSyncBackgroundTask_FireOneShotBackgroundSyncEvents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_runnable) {

  BackgroundSyncLauncherAndroid::Get()->FireBackgroundSyncEvents(
      blink::mojom::BackgroundSyncType::ONE_SHOT, j_runnable);
}

void JNI_PeriodicBackgroundSyncChromeWakeUpTask_FirePeriodicBackgroundSyncEvents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  BackgroundSyncLauncherAndroid::Get()->FireBackgroundSyncEvents(
      blink::mojom::BackgroundSyncType::PERIODIC, j_runnable);
}

void JNI_BackgroundSyncBackgroundTaskScheduler_SetPlayServicesVersionCheckDisabledForTests(
    JNIEnv* env,
    jboolean disabled) {
  BackgroundSyncLauncherAndroid::SetPlayServicesVersionCheckDisabledForTests(
      disabled);
}

// static
BackgroundSyncLauncherAndroid* BackgroundSyncLauncherAndroid::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return g_background_sync_launcher.Pointer();
}

// static
void BackgroundSyncLauncherAndroid::SetPlayServicesVersionCheckDisabledForTests(
    bool disabled) {
  disable_play_services_version_check_for_tests = disabled;
}

// static
void BackgroundSyncLauncherAndroid::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Get()->ScheduleBrowserWakeUpWithDelayImpl(sync_type, delay);
}

// static
void BackgroundSyncLauncherAndroid::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Get()->CancelBrowserWakeupImpl(sync_type);
}

// static
bool BackgroundSyncLauncherAndroid::ShouldDisableBackgroundSync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (disable_play_services_version_check_for_tests)
    return false;
  return Java_GooglePlayServicesChecker_shouldDisableBackgroundSync(
      base::android::AttachCurrentThread());
}

void BackgroundSyncLauncherAndroid::ScheduleBrowserWakeUpWithDelayImpl(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta soonest_wakeup_delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t min_delay_ms = soonest_wakeup_delta.InMilliseconds();

  Java_BackgroundSyncBackgroundTaskScheduler_scheduleOneOffTask(
      env, java_background_sync_background_task_scheduler_launcher_,
      GetBackgroundTaskType(sync_type), min_delay_ms);
}

void BackgroundSyncLauncherAndroid::CancelBrowserWakeupImpl(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();

  Java_BackgroundSyncBackgroundTaskScheduler_cancelOneOffTask(
      env, java_background_sync_background_task_scheduler_launcher_,
      GetBackgroundTaskType(sync_type));
}

void BackgroundSyncLauncherAndroid::FireBackgroundSyncEvents(
    blink::mojom::BackgroundSyncType sync_type,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);

  content::BackgroundSyncContext::FireBackgroundSyncEventsAcrossPartitions(
      profile, sync_type, j_runnable);
}

BackgroundSyncLauncherAndroid::BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();

  java_background_sync_background_task_scheduler_launcher_.Reset(
      Java_BackgroundSyncBackgroundTaskScheduler_getInstance(env));
}

BackgroundSyncLauncherAndroid::~BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}
