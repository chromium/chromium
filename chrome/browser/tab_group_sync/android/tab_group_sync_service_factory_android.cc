// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_sync/factory_jni_headers/TabGroupSyncServiceFactory_jni.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_TabGroupSyncServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  CHECK(profile);
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(service);
  return tab_groups::TabGroupSyncService::GetJavaObject(service);
}
