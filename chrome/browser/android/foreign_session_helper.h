// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_

#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

class TabAndroid;

namespace content {
class WebContents;
}  // namespace content

// TODO(crbug.com/40261558): Move this class to chrome/browser/recent_tabs
// module once dependency issues have been resolved.
class ForeignSessionHelper {
 public:
  explicit ForeignSessionHelper(Profile* profile);

  ForeignSessionHelper(const ForeignSessionHelper&) = delete;
  ForeignSessionHelper& operator=(const ForeignSessionHelper&) = delete;

  ~ForeignSessionHelper();

  void Destroy(JNIEnv* env);
  bool IsTabSyncEnabled(JNIEnv* env);
  void TriggerSessionSync(JNIEnv* env);
  void SetOnForeignSessionCallback(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& callback);
  bool GetForeignSessions(JNIEnv* env,
                          const base::android::JavaRef<jobject>& result);
  bool GetMobileAndTabletForeignSessions(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& result);
  bool OpenForeignSessionTab(JNIEnv* env,
                             TabAndroid* tab_android,
                             const std::string& session_tag,
                             int32_t tab_id,
                             int32_t disposition);
  void DeleteForeignSession(JNIEnv* env, const std::string& session_tag);
  void SetInvalidationsForSessionsEnabled(JNIEnv* env, bool enabled);
  int32_t OpenForeignSessionTabsAsBackgroundTabs(
      JNIEnv* env,
      TabAndroid* tab_android,
      const std::vector<int32_t>& session_tab_ids,
      const std::string& session_tag);

 private:
  // Fires |callback_| if it is not null.
  void FireForeignSessionCallback();
  // Returns the WebContents of the new foreground tab or nullptr if the
  // operation failed.
  content::WebContents* RestoreTabWithRenderer(const std::string& session_tag,
                                               TabAndroid* tab_android,
                                               int session_tab_id);
  // Returns whether a background tab with no renderer was restored.
  bool RestoreTabNoRenderer(const std::string& session_tag,
                            int session_tab_id,
                            content::WebContents* web_contents);

  raw_ptr<Profile> profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  base::CallbackListSubscription foreign_session_updated_subscription_;
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
