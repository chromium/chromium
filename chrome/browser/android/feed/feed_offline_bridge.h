// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/content/feed_offline_host.h"
#include "components/feed/core/content_metadata.h"

namespace feed {

class FeedOfflineHost;

// Native counterpart of FeedOfflineBridge.java. Holds non-owning pointers to
// native implementation, to which operations are delegated. Also capable of
// calling back into Java half.
class FeedOfflineBridge {
 public:
  FeedOfflineBridge(const base::android::JavaRef<jobject>& j_this,
                    FeedOfflineHost* offline_host);
  ~FeedOfflineBridge();

  void Destroy(JNIEnv* env, const base::android::JavaRef<jobject>& j_this);

  base::android::ScopedJavaLocalRef<jobject> GetOfflineId(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_url);

  void GetOfflineStatus(JNIEnv* env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobjectArray>& j_urls,
                        const base::android::JavaRef<jobject>& j_callback);

  void OnContentRemoved(JNIEnv* env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jobjectArray>& j_urls);

  void OnNewContentReceived(JNIEnv* env,
                            const base::android::JavaRef<jobject>& j_this);

  void OnNoListeners(JNIEnv* env,
                     const base::android::JavaRef<jobject>& j_this);

  // Used to convert from Java ContentMetadata to a native ContentMetadata, and
  // put the resulting object into |known_content_metadata_buffer_|. When a
  // GetKnownContent() call finishes, this method should be synchronously called
  // for every piece of data, and then OnGetKnownContentDone() should be called.
  void AppendContentMetadata(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_url,
      const base::android::JavaRef<jstring>& j_title,
      const jlong j_time_published_ms,
      const base::android::JavaRef<jstring>& j_image_url,
      const base::android::JavaRef<jstring>& j_publisher,
      const base::android::JavaRef<jstring>& j_favicon_url,
      const base::android::JavaRef<jstring>& j_snippet);

  // Called to flush the contents of |known_content_metadata_buffer_| to the
  // |offline_host_|. This should happen at the end of a GetKnownContent() call,
  // and after AppendContentMetadata() is called for all data.
  void OnGetKnownContentDone(JNIEnv* env,
                             const base::android::JavaRef<jobject>& j_this);

  void NotifyStatusChange(const std::string& url, bool available_offline);

 private:
  // Starts an the async request for ContentMetadata through KnownContentApi's
  // GetKnownContent(). Assumes the caller was FeedOfflineHost and will directly
  // call FeedOfflineHost::OnGetKnownContentDone() on async completion.
  void TriggerGetKnownContent();

  void OnGetOfflineStatus(base::android::ScopedJavaGlobalRef<jobject> callback,
                          std::vector<std::string> urls);

  // Reference to the Java half of this bridge. Always valid.
  base::android::ScopedJavaGlobalRef<jobject> j_this_;

  // Object to which all Java to native calls are delegated.
  FeedOfflineHost* offline_host_;

  // Temporarily holds ContentMetadata objects during the completion of a
  // GetKnownContent call.
  std::vector<ContentMetadata> known_content_metadata_buffer_;

  base::WeakPtrFactory<FeedOfflineBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedOfflineBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_OFFLINE_BRIDGE_H_
