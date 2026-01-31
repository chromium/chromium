// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_device_info/local_device_info_provider.h"
#include "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#include "components/sync_preferences/synced_set_up/android/pref_to_value_map_bridge.h"
#include "components/sync_preferences/synced_set_up/utils.h"

// Must come after all headers that specialize FromJniType, and must have no
// namespace.
#include "chrome/browser/sync/synced_set_up/android/jni_headers/SyncedSetUpUtilsBridge_jni.h"

using base::android::JavaRef;

namespace sync_preferences::synced_set_up {

/**
 * Retrieves the cross-device preferences from a remote device and populates the
 * given Java map.
 *
 * @param env The JNI environment.
 * @param profile The Profile to use.
 * @param cross_device_pref_tracker The CrossDevicePrefTracker to use.
 * @param map_bridge The PrefToValueMapBridge to populate.
 */
static void JNI_SyncedSetUpUtilsBridge_GetCrossDevicePrefsFromRemoteDevice(
    JNIEnv* env,
    int64_t profile,
    int64_t cross_device_pref_tracker,
    int64_t map_bridge) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(
          reinterpret_cast<Profile*>(profile));
  const syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  const syncer::DeviceInfo* local_device =
      device_info_sync_service->GetLocalDeviceInfoProvider()
          ->GetLocalDeviceInfo();

  std::map<std::string_view, base::Value> cross_device_prefs_map =
      sync_preferences::synced_set_up::GetCrossDevicePrefsFromRemoteDevice(
          reinterpret_cast<sync_preferences::CrossDevicePrefTracker*>(
              cross_device_pref_tracker),
          device_info_tracker, local_device);

  sync_preferences::synced_set_up::PrefToValueMapBridge* pref_map =
      reinterpret_cast<sync_preferences::synced_set_up::PrefToValueMapBridge*>(
          map_bridge);
  for (auto const& [key, val] : cross_device_prefs_map) {
    switch (val.type()) {
      case base::Value::Type::BOOLEAN:
        pref_map->PutBooleanValue(env, std::string(key), val.GetBool());
        break;
      case base::Value::Type::INTEGER:
        pref_map->PutIntValue(env, std::string(key), val.GetInt());
        break;
      case base::Value::Type::STRING:
        pref_map->PutStringValue(env, std::string(key), val.GetString());
        break;
      default:
        // Other types not supported by PrefToValueMapBridge.
        break;
    }
  }
}

}  // namespace sync_preferences::synced_set_up

DEFINE_JNI(SyncedSetUpUtilsBridge)
