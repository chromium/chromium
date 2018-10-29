// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_IMAGE_LOADER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_IMAGE_LOADER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"

namespace feed {

class FeedImageManager;

// Native counterpart of FeedImageLoaderBridge.java. Holds non-owning pointers
// to native implementation, to which operations are delegated. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedImageLoaderBridge {
 public:
  explicit FeedImageLoaderBridge(FeedImageManager* feed_image_manager);
  ~FeedImageLoaderBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void FetchImage(JNIEnv* j_env,
                  const base::android::JavaRef<jobject>& j_this,
                  const base::android::JavaRef<jstring>& j_url,
                  const jint width_px,
                  const jint height_px,
                  const base::android::JavaRef<jobject>& j_callback);

 private:
  void OnImageFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                      const gfx::Image& image,
                      size_t ignored);

  FeedImageManager* feed_image_manager_;

  base::WeakPtrFactory<FeedImageLoaderBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedImageLoaderBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_IMAGE_LOADER_BRIDGE_H_
