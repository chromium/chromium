// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_enterprise_helper.h"

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwEnterpriseHelper_jni.h"

namespace android_webview::enterprise {

void GetEnterpriseState(EnterpriseStateCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {}, base::BindOnce([]() {
        JNIEnv* env = base::android::AttachCurrentThread();
        return static_cast<EnterpriseState>(
            Java_AwEnterpriseHelper_getEnterpriseState(env));
      }),
      std::move(callback));
}

}  // namespace android_webview::enterprise
