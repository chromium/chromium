// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFETY_HUB_ANDROID_UNUSED_SITE_PERMISSIONS_BRIDGE_H_
#define CHROME_BROWSER_SAFETY_HUB_ANDROID_UNUSED_SITE_PERMISSIONS_BRIDGE_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/safety_hub/unused_site_permissions_service.h"

class Profile;

// JNI helper methods to enable unit testing.
PermissionsData FromJavaPermissionsData(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobject);

base::android::ScopedJavaLocalRef<jobject> ToJavaPermissionsData(
    JNIEnv* env,
    const PermissionsData& obj);

std::vector<PermissionsData> GetRevokedPermissions(Profile* profile);

void RegrantPermissions(Profile* profile, std::string& primary_pattern);

void UndoRegrantPermissions(Profile* profile,
                            PermissionsData& permissions_data);

void ClearRevokedPermissionsReviewList(Profile* profile);

void RestoreRevokedPermissionsReviewList(
    Profile* profile,
    std::vector<PermissionsData>& permissions_data_list);

namespace jni_zero {

template <>
inline PermissionsData FromJniType<PermissionsData>(
    JNIEnv* env,
    const JavaRef<jobject>& jobject) {
  return FromJavaPermissionsData(env, jobject);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env,
                                             const PermissionsData& obj) {
  return ToJavaPermissionsData(env, obj);
}

}  // namespace jni_zero

#endif  // CHROME_BROWSER_SAFETY_HUB_ANDROID_UNUSED_SITE_PERMISSIONS_BRIDGE_H_
