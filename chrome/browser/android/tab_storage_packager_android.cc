// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_storage_packager_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/token.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/collection_storage_package.h"
#include "chrome/browser/tab/storage_id_mapping.h"
#include "chrome/browser/tab/tab_storage_package.h"
#include "chrome/browser/tab/tab_storage_packager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab/jni_headers/TabStoragePackager_jni.h"

namespace tabs {
static const int kTabStoragePackagerAndroidVersion = 1;

TabStoragePackagerAndroid::TabStoragePackagerAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      Java_TabStoragePackager_create(env, reinterpret_cast<intptr_t>(this)));
}

void TabStoragePackagerAndroid::Package(const TabInterface* tab) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabStoragePackager_packageTab(env, java_obj_,
                                     static_cast<const TabAndroid*>(tab));
}

void TabStoragePackagerAndroid::Package(const TabCollection* collection,
                                        StorageIdMapping& mapping) {
  // TODO(https://crbug.com/448875689): Fill this package with relevant data.
  package_ = std::make_unique<CollectionStoragePackage>();
}

void TabStoragePackagerAndroid::ConsolidatePackageData(
    JNIEnv* env,
    jlong timestamp_millis,
    const jni_zero::JavaParamRef<jobject>& web_contents_state_buffer,
    std::string& opener_app_id,
    jint theme_color,
    jlong last_navigation_committed_timestamp_millis,
    jboolean tab_has_sensitive_content,
    TabAndroid* tab) {
  DCHECK(!package_) << "Previous package was not released.";

  std::unique_ptr<std::string> web_contents_state_bytes;
  if (web_contents_state_buffer) {
    base::span<const uint8_t> span =
        base::android::JavaByteBufferToSpan(env, web_contents_state_buffer);
    web_contents_state_bytes =
        std::make_unique<std::string>(span.begin(), span.end());
  }

  base::Token tab_group_id;
  if (tab->GetGroup().has_value()) {
    tab_group_id = tab->GetGroup()->token();
  }

  std::unique_ptr<AndroidTabPackage> android_package =
      std::make_unique<AndroidTabPackage>(
          kTabStoragePackagerAndroidVersion, tab->GetAndroidId(),
          tab->GetParentId(), timestamp_millis,
          std::move(web_contents_state_bytes),
          std::make_unique<std::string>(std::move(opener_app_id)), theme_color,
          last_navigation_committed_timestamp_millis, tab_has_sensitive_content,
          tab->GetTabLaunchTypeAtCreation());

  package_ = std::make_unique<TabStoragePackage>(
      tab->GetUserAgent(), std::move(tab_group_id), tab->IsPinned(),
      std::move(android_package));
}

std::unique_ptr<StoragePackage> TabStoragePackagerAndroid::ReleasePackage() {
  DCHECK(package_) << "Package was not instantiated.";
  return std::move(package_);
}

TabStoragePackagerAndroid::~TabStoragePackagerAndroid() = default;

}  // namespace tabs
