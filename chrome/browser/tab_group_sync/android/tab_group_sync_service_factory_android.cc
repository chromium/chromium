// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/tab_group_sync/jni_headers/TabGroupSyncServiceFactory_jni.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_TabGroupSyncServiceFactory_GetForProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  CHECK(profile);
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(service);
  return tab_groups::TabGroupSyncService::GetJavaObject(service);
}
