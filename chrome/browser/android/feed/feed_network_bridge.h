// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_NETWORK_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_NETWORK_BRIDGE_H_

#include <jni.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace feed {

class FeedNetworkingHost;

// Native counterpart of FeedNetworkBridge.java. Holds non-owning pointers to
// native implementation, to which operations are delegated. Results are passed
// back by a single argument callback so base::android::RunObjectCallbackAndroid
// can be used. This bridge is instantiated, owned, and destroyed from Java.
class FeedNetworkBridge {
 public:
  explicit FeedNetworkBridge(
      const base::android::JavaParamRef<jobject>& j_profile);
  ~FeedNetworkBridge();

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& j_this);

  void SendNetworkRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_this,
      const base::android::JavaParamRef<jstring>& j_url,
      const base::android::JavaParamRef<jstring>& j_request_type,
      const base::android::JavaParamRef<jbyteArray>& j_body,
      const base::android::JavaParamRef<jobject>& j_callback);

  void CancelRequests(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_this);

 private:
  void OnResult(const base::android::ScopedJavaGlobalRef<jobject>& j_callback,
                int32_t http_code,
                std::vector<uint8_t> response_bytes);

  FeedNetworkingHost* networking_host_;

  base::WeakPtrFactory<FeedNetworkBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedNetworkBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_NETWORK_BRIDGE_H_
