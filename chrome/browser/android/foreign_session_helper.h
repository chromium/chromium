// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

using base::android::ScopedJavaLocalRef;

class ForeignSessionHelper {
 public:
  explicit ForeignSessionHelper(Profile* profile);
  ~ForeignSessionHelper();

  void Destroy(JNIEnv* env);
  jboolean IsTabSyncEnabled(JNIEnv* env);
  void TriggerSessionSync(JNIEnv* env);
  void SetOnForeignSessionCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& callback);
  jboolean GetForeignSessions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& result);
  jboolean OpenForeignSessionTab(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_tab,
      const base::android::JavaParamRef<jstring>& session_tag,
      jint tab_id,
      jint disposition);
  void DeleteForeignSession(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& session_tag);
  void SetInvalidationsForSessionsEnabled(
      JNIEnv* env,
      jboolean enabled);

 private:
  // Fires |callback_| if it is not null.
  void FireForeignSessionCallback();

  Profile* profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  std::unique_ptr<base::CallbackList<void()>::Subscription>
      foreign_session_updated_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
