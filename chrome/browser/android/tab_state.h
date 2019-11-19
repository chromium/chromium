// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_STATE_H_
#define CHROME_BROWSER_ANDROID_TAB_STATE_H_

#include <vector>

#include "base/android/scoped_java_ref.h"

namespace sessions {
class SerializedNavigationEntry;
}

class TabAndroid;

// Stores state for a WebContents, including its navigation history.
class WebContentsState {
 public:
  using DeletionPredicate = base::RepeatingCallback<bool(
      const sessions::SerializedNavigationEntry& entry)>;

  static base::android::ScopedJavaLocalRef<jobject>
      GetContentsStateAsByteBuffer(JNIEnv* env, TabAndroid* tab);

  // Returns a new buffer without the navigations matching |predicate|.
  // Returns null if no deletions happened.
  static base::android::ScopedJavaLocalRef<jobject>
  DeleteNavigationEntriesFromByteBuffer(JNIEnv* env,
                                        void* data,
                                        int size,
                                        int saved_state_version,
                                        const DeletionPredicate& predicate);

  // Extracts display title from serialized tab data on restore
  static base::android::ScopedJavaLocalRef<jstring>
      GetDisplayTitleFromByteBuffer(JNIEnv* env, void* data,
                                    int size, int saved_state_version);

  // Extracts virtual url from serialized tab data on restore
  static base::android::ScopedJavaLocalRef<jstring>
      GetVirtualUrlFromByteBuffer(JNIEnv* env, void* data,
                                  int size, int saved_state_version);

  // Restores a WebContents from the passed in state.
  static base::android::ScopedJavaLocalRef<jobject>
  RestoreContentsFromByteBuffer(JNIEnv* env,
                                jobject state,
                                jint saved_state_version,
                                jboolean initially_hidden);

  // Synthesizes a stub, single-navigation state for a tab that will be loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject>
  CreateSingleNavigationStateAsByteBuffer(JNIEnv* env,
                                          jstring url,
                                          jstring referrer_url,
                                          jint referrer_policy,
                                          jstring initiator_origin,
                                          jboolean is_off_the_record);
};

#endif  // CHROME_BROWSER_ANDROID_TAB_STATE_H_
