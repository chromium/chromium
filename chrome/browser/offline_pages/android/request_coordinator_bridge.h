// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_REQUEST_COORDINATOR_BRIDGE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_REQUEST_COORDINATOR_BRIDGE_H_

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace offline_pages {

class SavePageRequest;

namespace android {

base::android::ScopedJavaLocalRef<jobjectArray> CreateJavaSavePageRequests(
    JNIEnv* env,
    const std::vector<std::unique_ptr<SavePageRequest>>& requests);

}  // namespace android
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_REQUEST_COORDINATOR_BRIDGE_H_
