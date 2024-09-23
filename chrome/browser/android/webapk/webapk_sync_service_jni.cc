// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkSyncService_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace webapk {

namespace {

// Called after getting restorable app info.
void OnGotAppsInfo(const JavaRef<jobject>& java_callback,
                   const std::vector<std::string>& app_ids,
                   const std::vector<std::u16string>& names,
                   const std::vector<int>& last_used_in_days,
                   const std::vector<SkBitmap>& icons) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmaps =
      Java_WebApkSyncService_createBitmapList(env);
  for (const SkBitmap& bitmap : icons) {
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(bitmap);
    Java_WebApkSyncService_addToBitmapList(env, jbitmaps, jbitmap);
  }
  Java_PwaRestorableListCallback_onRestorableAppsAvailable(
      env, java_callback, true,
      base::android::ToJavaArrayOfStrings(env, app_ids),
      base::android::ToJavaArrayOfStrings(env, names),
      base::android::ToJavaIntArray(env, last_used_in_days), jbitmaps);
}

}  // namespace

static void JNI_WebApkSyncService_OnWebApkUsed(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& java_webapk_specifics,
    jboolean is_install) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  std::string specifics_bytes;
  base::android::JavaByteArrayToString(env, java_webapk_specifics,
                                       &specifics_bytes);

  std::unique_ptr<sync_pb::WebApkSpecifics> specifics =
      std::make_unique<sync_pb::WebApkSpecifics>();
  if (!specifics->ParseFromString(specifics_bytes)) {
    LOG(ERROR) << "failed to parse WebApkSpecifics proto";
    return;
  }
  WebApkSyncServiceFactory::GetForProfile(profile)->OnWebApkUsed(
      std::move(specifics), static_cast<bool>(is_install));
}

static void JNI_WebApkSyncService_OnWebApkUninstalled(
    JNIEnv* env,
    std::string& java_manifest_id) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  WebApkSyncServiceFactory::GetForProfile(profile)->OnWebApkUninstalled(
      java_manifest_id);
}

static void JNI_WebApkSyncService_RemoveOldWebAPKsFromSync(
    JNIEnv* env,
    jlong java_current_time_ms_since_unix_epoch) {
  if (!base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return;
  }

  WebApkSyncServiceFactory::GetForProfile(profile)->RemoveOldWebAPKsFromSync(
      static_cast<int64_t>(java_current_time_ms_since_unix_epoch));
}

static void JNI_WebApkSyncService_FetchRestorableApps(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& java_callback) {
  if (profile == nullptr ||
      !base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    return;
  }

  ScopedJavaGlobalRef<jobject> callback_ref(java_callback);
  WebApkSyncServiceFactory::GetForProfile(profile)->PrepareRestorableAppsInfo(
      base::BindOnce(&OnGotAppsInfo, callback_ref));
}

}  // namespace webapk
