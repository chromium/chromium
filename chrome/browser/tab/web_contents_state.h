// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
#define CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"

namespace sessions {
class SerializedNavigationEntry;
}

namespace content {
class WebContents;
}  // namespace content

// Stores state for a WebContents, including its navigation history.
class WebContentsState {
 public:
  using DeletionPredicate = base::RepeatingCallback<bool(
      const sessions::SerializedNavigationEntry& entry)>;

  static base::android::ScopedJavaLocalRef<jobject>
  GetContentsStateAsByteBuffer(JNIEnv* env, content::WebContents* web_contents);

  // Returns a new buffer without the navigations matching |predicate|.
  // Returns null if no deletions happened.
  static base::android::ScopedJavaLocalRef<jobject>
  DeleteNavigationEntriesFromByteBuffer(JNIEnv* env,
                                        void* data,
                                        int size,
                                        int saved_state_version,
                                        const DeletionPredicate& predicate);

  // Extracts display title from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring>
  GetDisplayTitleFromByteBuffer(JNIEnv* env,
                                void* data,
                                int size,
                                int saved_state_version);

  // Extracts virtual url from serialized tab data on restore.
  static base::android::ScopedJavaLocalRef<jstring> GetVirtualUrlFromByteBuffer(
      JNIEnv* env,
      void* data,
      int size,
      int saved_state_version);

  // Restores a WebContents from the passed in state using JNI parameters.
  static base::android::ScopedJavaLocalRef<jobject>
  RestoreContentsFromByteBuffer(JNIEnv* env,
                                jobject state,
                                jint saved_state_version,
                                jboolean initially_hidden,
                                jboolean no_renderer);

  // Restores a WebContents from the passed in state using native parameters.
  static std::unique_ptr<content::WebContents> RestoreContentsFromByteBuffer(
      void* data,
      int size,
      int saved_state_version,
      bool initially_hidden,
      bool no_renderer);

  // Synthesizes a stub, single-navigation state for a tab that will be loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject>
  CreateSingleNavigationStateAsByteBuffer(
      JNIEnv* env,
      jstring url,
      jstring referrer_url,
      jint referrer_policy,
      const base::android::JavaParamRef<jobject>& initiator_origin,
      jboolean is_off_the_record);
};

#endif  // CHROME_BROWSER_TAB_WEB_CONTENTS_STATE_H_
