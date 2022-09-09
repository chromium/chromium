// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_
#define CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"

namespace android {

std::vector<std::string> GetBackupPrefNames();

// Test interface wrapping the static functions that are only called from Java.
base::android::ScopedJavaLocalRef<jobjectArray> GetBoolBackupNamesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller);
base::android::ScopedJavaLocalRef<jbooleanArray> GetBoolBackupValuesForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller);
void SetBoolBackupPrefsForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobjectArray>& names,
    const base::android::JavaParamRef<jbooleanArray>& values);

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_
