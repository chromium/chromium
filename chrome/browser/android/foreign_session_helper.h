// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "content/public/browser/web_contents.h"

using base::android::ScopedJavaLocalRef;

// TODO(crbug.com/40261558): Move this class to chrome/browser/recent_tabs
// module once dependency issues have been resolved.
class ForeignSessionHelper {
 public:
  explicit ForeignSessionHelper(Profile* profile);

  ForeignSessionHelper(const ForeignSessionHelper&) = delete;
  ForeignSessionHelper& operator=(const ForeignSessionHelper&) = delete;

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
  jboolean GetMobileAndTabletForeignSessions(
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
  void SetInvalidationsForSessionsEnabled(JNIEnv* env, jboolean enabled);
  jint OpenForeignSessionTabsAsBackgroundTabs(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_tab,
      const base::android::JavaParamRef<jintArray>& j_session_tab_ids,
      const base::android::JavaParamRef<jstring>& session_tag);

 private:
  // Fires |callback_| if it is not null.
  void FireForeignSessionCallback();
  // Returns whether a foreground tab with renderer was restored.
  bool RestoreTabWithRenderer(
      const base::android::JavaParamRef<jstring>& session_tag,
      const base::android::JavaParamRef<jobject>& j_tab,
      int session_tab_id);
  // Returns whether a background tab with no renderer was restored.
  bool RestoreTabNoRenderer(
      const base::android::JavaParamRef<jstring>& session_tag,
      int session_tab_id,
      content::WebContents* web_contents);

  raw_ptr<Profile> profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  base::CallbackListSubscription foreign_session_updated_subscription_;
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
