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

#endif  // CHROME_BROWSER_SAFETY_HUB_ANDROID_UNUSED_SITE_PERMISSIONS_BRIDGE_H_
