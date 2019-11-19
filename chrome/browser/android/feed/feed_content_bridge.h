// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_CONTENT_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_CONTENT_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/feed/core/feed_content_database.h"

namespace feed {

class FeedContentDatabase;

// Native counterpart of FeedContentBridge.java. Holds non-owning pointers
// to native implementation to which operations are delegated. Results are
// passed back by a single argument callback so
// base::android::RunBooleanCallbackAndroid() and
// base::android::RunObjectCallbackAndroid() can be used. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedContentBridge {
 public:
  explicit FeedContentBridge(FeedContentDatabase* feed_Storage_database);
  ~FeedContentBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void LoadContent(JNIEnv* j_env,
                   const base::android::JavaRef<jobject>& j_this,
                   const base::android::JavaRef<jobjectArray>& j_keys,
                   const base::android::JavaRef<jobject>& j_success_callback,
                   const base::android::JavaRef<jobject>& j_failure_callback);
  void LoadContentByPrefix(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_prefix,
      const base::android::JavaRef<jobject>& j_success_callback,
      const base::android::JavaRef<jobject>& j_failure_callback);
  void LoadAllContentKeys(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jobject>& j_success_callback,
      const base::android::JavaRef<jobject>& j_failure_callback);
  void CommitContentMutation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const base::android::JavaRef<jobject>& j_callback);

  void CreateContentMutation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);
  void DeleteContentMutation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);
  void AppendDeleteOperation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const base::android::JavaRef<jstring>& j_key);
  void AppendDeleteByPrefixOperation(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_prefix);
  void AppendUpsertOperation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const base::android::JavaRef<jstring>& j_key,
                             const base::android::JavaRef<jbyteArray>& j_data);
  void AppendDeleteAllOperation(JNIEnv* j_env,
                                const base::android::JavaRef<jobject>& j_this);

 private:
  void OnLoadContentDone(
      base::android::ScopedJavaGlobalRef<jobject> success_callback,
      base::android::ScopedJavaGlobalRef<jobject> failure_callback,
      bool success,
      std::vector<FeedContentDatabase::KeyAndData> pairs);
  void OnLoadAllContentKeysDone(
      base::android::ScopedJavaGlobalRef<jobject> success_callback,
      base::android::ScopedJavaGlobalRef<jobject> failure_callback,
      bool success,
      std::vector<std::string> keys);
  void OnStorageCommitDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                           bool success);

  // This unique_ptr will hold a list of ContentOperations which are not
  // committed yet. After commit to database, this unique_ptr will be reset.
  std::unique_ptr<ContentMutation> content_mutation_;

  FeedContentDatabase* feed_content_database_;

  base::WeakPtrFactory<FeedContentBridge> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedContentBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_CONTENT_BRIDGE_H_
