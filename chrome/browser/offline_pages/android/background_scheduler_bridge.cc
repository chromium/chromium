// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/background_scheduler_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "chrome/android/chrome_jni_headers/BackgroundSchedulerBridge_jni.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/offline_pages/core/background/device_conditions.h"
#include "components/offline_pages/core/background/offliner.h"
#include "components/offline_pages/core/background/request_coordinator.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace offline_pages {
namespace android {

// JNI call to start request processing in scheduled mode.
static jboolean JNI_BackgroundSchedulerBridge_StartScheduledProcessing(
    JNIEnv* env,
    const jboolean j_power_connected,
    const jint j_battery_percentage,
    const jint j_net_connection_type,
    const JavaParamRef<jobject>& j_callback_obj) {
  ScopedJavaGlobalRef<jobject> j_callback_ref;
  j_callback_ref.Reset(env, j_callback_obj);

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return false;

  // Make sure the auto-fetch service is running, so it can respond to completed
  // pages.
  OfflinePageAutoFetcherServiceFactory::GetForBrowserContext(profile);

  // Lookup/create RequestCoordinator KeyedService and call
  // StartScheduledProcessing on it with bound j_callback_obj.
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetInstance()->
      GetForBrowserContext(profile);
  DVLOG(2) << "resource_coordinator: " << coordinator;
  DeviceConditions device_conditions(
      j_power_connected, j_battery_percentage,
      static_cast<net::NetworkChangeNotifier::ConnectionType>(
          j_net_connection_type));
  return coordinator->StartScheduledProcessing(
      device_conditions,
      base::BindRepeating(&base::android::RunBooleanCallbackAndroid,
                          j_callback_ref));
}

// JNI call to stop request processing in scheduled mode.
static void JNI_BackgroundSchedulerBridge_StopScheduledProcessing(JNIEnv* env) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;
  RequestCoordinator* coordinator =
      RequestCoordinatorFactory::GetInstance()->GetForBrowserContext(profile);
  DVLOG(2) << "resource_coordinator: " << coordinator;
  if (!coordinator)
    return;
  coordinator->CancelProcessing();
}

BackgroundSchedulerBridge::BackgroundSchedulerBridge() = default;

BackgroundSchedulerBridge::~BackgroundSchedulerBridge() = default;

void BackgroundSchedulerBridge::Schedule(
    const TriggerConditions& trigger_conditions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_conditions =
      CreateTriggerConditions(env, trigger_conditions.require_power_connected,
                              trigger_conditions.minimum_battery_percentage,
                              trigger_conditions.require_unmetered_network);
  Java_BackgroundSchedulerBridge_schedule(env, j_conditions);
}

void BackgroundSchedulerBridge::BackupSchedule(
    const TriggerConditions& trigger_conditions,
    int64_t delay_in_seconds) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_conditions =
      CreateTriggerConditions(env, trigger_conditions.require_power_connected,
                              trigger_conditions.minimum_battery_percentage,
                              trigger_conditions.require_unmetered_network);
  Java_BackgroundSchedulerBridge_backupSchedule(
      env, j_conditions, delay_in_seconds);
}

void BackgroundSchedulerBridge::Unschedule() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSchedulerBridge_unschedule(env);
}

ScopedJavaLocalRef<jobject> BackgroundSchedulerBridge::CreateTriggerConditions(
    JNIEnv* env,
    bool require_power_connected,
    int minimum_battery_percentage,
    bool require_unmetered_network) const {
  return Java_BackgroundSchedulerBridge_createTriggerConditions(
      env, require_power_connected, minimum_battery_percentage,
      require_unmetered_network);
}

const DeviceConditions&
BackgroundSchedulerBridge::GetCurrentDeviceConditions() {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Call the JNI methods to get the device conditions we need.
  jboolean jpower = Java_BackgroundSchedulerBridge_getPowerConditions(env);
  jint jbattery = Java_BackgroundSchedulerBridge_getBatteryConditions(env);
  jint jnetwork = Java_BackgroundSchedulerBridge_getNetworkConditions(env);

  // Cast the java types back to the types we use.
  bool power = static_cast<bool>(jpower);
  int battery = static_cast<int>(jbattery);

  net::NetworkChangeNotifier::ConnectionType network_connection_type =
      static_cast<net::NetworkChangeNotifier::ConnectionType>(jnetwork);

  // Now return the current conditions to the caller.
  device_conditions_ = std::make_unique<DeviceConditions>(
      power, battery, network_connection_type);
  return *device_conditions_;
}

}  // namespace android
}  // namespace offline_pages
