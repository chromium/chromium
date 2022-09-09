// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_INFO_H_
#define CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_INFO_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace history {
class DeletionInfo;
}  // namespace history

// Create a Java wrapper object of history::DeletionInfo to pass over the JNI.
base::android::ScopedJavaLocalRef<jobject> CreateHistoryDeletionInfo(
    JNIEnv* env,
    const history::DeletionInfo* deletion_info);

#endif  // CHROME_BROWSER_ANDROID_HISTORY_HISTORY_DELETION_INFO_H_
