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

namespace sessions {
struct SessionTab;
struct SessionWindow;
}  // namespace sessions

namespace sync_sessions {
class OpenTabsUIDelegate;
class SyncedSession;
}  // namespace sync_sessions

// TODO(crbug.com/40261558): Move this class to chrome/browser/recent_tabs
// module once dependency issues have been resolved.
class ForeignSessionHelper {
 public:
  static bool ShouldSkipTab(const sessions::SessionTab& session_tab);
  static bool ShouldSkipWindow(const sessions::SessionWindow& window);
  static bool ShouldSkipSession(const sync_sessions::SyncedSession& session);

  explicit ForeignSessionHelper(Profile* profile);

  ForeignSessionHelper(const ForeignSessionHelper&) = delete;
  ForeignSessionHelper& operator=(const ForeignSessionHelper&) = delete;

  ~ForeignSessionHelper();

  bool IsTabSyncEnabled();
  void TriggerSessionSync();
  void SetOnForeignSessionCallback(
      const base::android::JavaRef<jobject>& callback);
  bool GetForeignSessions(JNIEnv* env,
                          const base::android::JavaRef<jobject>& result);
  bool GetMobileAndTabletForeignSessions(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& result);
  bool OpenForeignSessionTab(TabAndroid* tab_android,
                             const std::string& session_tag,
                             int32_t tab_id,
                             int32_t disposition);
  void DeleteForeignSession(const std::string& session_tag);
  void SetInvalidationsForSessionsEnabled(bool enabled);
  // Restores the foreign tabs in `session_tab_ids` into the tab model of
  // `tab_android`. If `all_in_background` is true, every tab restores as a
  // background tab without a renderer and `tab_android` stays in place as the
  // parent. Otherwise the first tab that restores replaces `tab_android` and
  // the rest restore as background tabs without a renderer. Returns one entry
  // per input ID, in input order: the Android ID of the restored tab, or
  // `TabAndroid::kInvalidTabId` if that tab was not restored.
  std::vector<int32_t> OpenForeignSessionTabsAsBackgroundTabs(
      TabAndroid* tab_android,
      const std::vector<int32_t>& session_tab_ids,
      const std::string& session_tag,
      bool all_in_background);

 private:
  // Fires |callback_| if it is not null.
  void FireForeignSessionCallback();
  // Returns the WebContents of the new foreground tab or nullptr if the
  // operation failed.
  content::WebContents* RestoreTabWithRenderer(
      sync_sessions::OpenTabsUIDelegate* open_tabs,
      const std::string& session_tag,
      TabAndroid* tab_android,
      int session_tab_id);
  // Returns the WebContents of the new background tab with no renderer or
  // nullptr if the operation failed.
  content::WebContents* RestoreTabNoRenderer(
      sync_sessions::OpenTabsUIDelegate* open_tabs,
      const std::string& session_tag,
      int session_tab_id,
      content::WebContents* web_contents);

  raw_ptr<Profile> profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  base::CallbackListSubscription foreign_session_updated_subscription_;
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
