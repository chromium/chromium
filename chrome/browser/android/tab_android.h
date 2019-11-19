// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/tab_state.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/sessions/core/session_id.h"

class GURL;
class Profile;

namespace cc {
class Layer;
}

namespace android {
class TabWebContentsDelegateAndroid;
class TabContentManager;
}

namespace content {
class DevToolsAgentHost;
class WebContents;
}

class TabAndroid {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
  enum TabLoadStatus {
    PAGE_LOAD_FAILED = 0,
    DEFAULT_PAGE_LOAD = 1,
    PARTIAL_PRERENDERED_PAGE_LOAD = 2,
    FULL_PRERENDERED_PAGE_LOAD = 3,
  };

  // Convenience method to retrieve the Tab associated with the passed
  // WebContents.  Can return NULL.
  static TabAndroid* FromWebContents(const content::WebContents* web_contents);

  // Returns the native TabAndroid stored in the Java Tab represented by
  // |obj|.
  static TabAndroid* GetNativeTab(JNIEnv* env,
                                  const base::android::JavaRef<jobject>& obj);

  // Function to attach helpers to the contentView.
  static void AttachTabHelpers(content::WebContents* web_contents);

  TabAndroid(JNIEnv* env, const base::android::JavaRef<jobject>& obj);
  ~TabAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Return the WebContents, if any, currently owned by this TabAndroid.
  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Return the cc::Layer that represents the content for this TabAndroid.
  scoped_refptr<cc::Layer> GetContentLayer() const;

  // Return specific id information regarding this TabAndroid.
  const SessionID& window_id() const { return session_window_id_; }

  int GetAndroidId() const;
  int GetSyncId() const;
  bool IsNativePage() const;

  // Return the tab title.
  base::string16 GetTitle() const;

  // Return the tab url.
  GURL GetURL() const;

  // Return whether the tab is currently visible and the user can interact with
  // it.
  bool IsUserInteractable() const;

  // Helper methods to make it easier to access objects from the associated
  // WebContents.  Can return NULL.
  Profile* GetProfile() const;
  sync_sessions::SyncedTabDelegate* GetSyncedTabDelegate() const;

  // Delete navigation entries matching predicate from frozen state.
  void DeleteFrozenNavigationEntries(
      const WebContentsState::DeletionPredicate& predicate);

  void SetWindowSessionID(SessionID window_id);
  void SetSyncId(int sync_id);

  // Methods called from Java via JNI -----------------------------------------

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void InitWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean incognito,
      jboolean is_background_tab,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint jparent_tab_id,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
      const base::android::JavaParamRef<jobject>& jcontext_menu_populator);
  void UpdateDelegates(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& obj,
        const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
        const base::android::JavaParamRef<jobject>& jcontext_menu_populator);
  void DestroyWebContents(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void ReleaseWebContents(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  TabLoadStatus LoadUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& j_initiator_origin,
      const base::android::JavaParamRef<jstring>& j_extra_headers,
      const base::android::JavaParamRef<jobject>& j_post_data,
      jint page_transition,
      const base::android::JavaParamRef<jstring>& j_referrer_url,
      jint referrer_policy,
      jboolean is_renderer_initiated,
      jboolean should_replace_current_entry,
      jboolean has_user_gesture,
      jboolean should_clear_history_list,
      jlong omnibox_input_received_timestamp,
      jlong intent_received_timestamp);
  void SetActiveNavigationEntryTitleForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle);

  // Called to get default favicon of current tab, return null if no
  // favicon is avaliable for current tab.
  base::android::ScopedJavaLocalRef<jobject> GetFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void LoadOriginalImage(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void SetInterceptNavigationDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& delegate);

  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost();

  void SetDevToolsAgentHost(scoped_refptr<content::DevToolsAgentHost> host);

 private:
  JavaObjectWeakGlobalRef weak_java_tab_;

  // Identifier of the window the tab is in.
  SessionID session_window_id_;

  scoped_refptr<cc::Layer> content_layer_;
  android::TabContentManager* tab_content_manager_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<android::TabWebContentsDelegateAndroid>
      web_contents_delegate_;
  scoped_refptr<content::DevToolsAgentHost> devtools_host_;
  std::unique_ptr<browser_sync::SyncedTabDelegateAndroid> synced_tab_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TabAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
