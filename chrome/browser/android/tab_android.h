// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab/web_contents_state.h"
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
}

namespace content {
class DevToolsAgentHost;
class WebContents;
}  // namespace content

class TabAndroid : public base::SupportsUserData {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when WebContents is initialized.
    virtual void OnInitWebContents(TabAndroid* tab) = 0;
  };

  // Convenience method to retrieve the Tab associated with the passed
  // WebContents.  Can return NULL.
  static TabAndroid* FromWebContents(const content::WebContents* web_contents);

  // Returns the native TabAndroid stored in the Java Tab represented by
  // |obj|.
  static TabAndroid* GetNativeTab(JNIEnv* env,
                                  const base::android::JavaRef<jobject>& obj);

  // Returns the a vector of native TabAndroid stored in the Java Tab array
  // represented by |obj_array|.
  static std::vector<TabAndroid*> GetAllNativeTabs(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jobjectArray>& obj_array);

  // Function to attach helpers to the contentView.
  static void AttachTabHelpers(content::WebContents* web_contents);

  TabAndroid(JNIEnv* env, const base::android::JavaRef<jobject>& obj);

  TabAndroid(const TabAndroid&) = delete;
  TabAndroid& operator=(const TabAndroid&) = delete;

  ~TabAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Return the WebContents, if any, currently owned by this TabAndroid.
  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Return the cc::Layer that represents the content for this TabAndroid.
  scoped_refptr<cc::Layer> GetContentLayer() const;

  // Return specific id information regarding this TabAndroid.
  const SessionID& window_id() const { return session_window_id_; }

  int GetAndroidId() const;
  bool IsNativePage() const;
  int GetLaunchType() const;
  int GetUserAgent() const;

  // Return the tab title.
  std::u16string GetTitle() const;

  // Return the tab url.
  GURL GetURL() const;

  // Return whether the tab is currently visible and the user can interact with
  // it.
  bool IsUserInteractable() const;

  // Helper methods to make it easier to access objects from the associated
  // WebContents.  Can return NULL.
  Profile* GetProfile() const;
  sync_sessions::SyncedTabDelegate* GetSyncedTabDelegate() const;

  // Whether this tab is an incognito tab. Prefer
  // `GetProfile()->IsOffTheRecord()` unless `web_contents()` is nullptr.
  bool IsIncognito() const;

  // Delete navigation entries matching predicate from frozen state.
  void DeleteFrozenNavigationEntries(
      const WebContentsState::DeletionPredicate& predicate);

  void SetWindowSessionID(SessionID window_id);

  std::unique_ptr<content::WebContents> SwapWebContents(
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load);

  bool IsCustomTab();
  bool IsHidden();

  static bool isHardwareKeyboardAvailable(raw_ptr<TabAndroid> tab_android);

  // Observers -----------------------------------------------------------------

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods called from Java via JNI -----------------------------------------

  void Destroy(JNIEnv* env);
  void InitWebContents(
      JNIEnv* env,
      jboolean incognito,
      jboolean is_background_tab,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
      const base::android::JavaParamRef<jobject>&
          jcontext_menu_populator_factory);
  void UpdateDelegates(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents_delegate,
      const base::android::JavaParamRef<jobject>&
          jcontext_menu_populator_factory);
  void DestroyWebContents(JNIEnv* env);
  void ReleaseWebContents(JNIEnv* env);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  void SetActiveNavigationEntryTitleForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle);

  void LoadOriginalImage(JNIEnv* env);
  void OnShow(JNIEnv* env);

  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost();

  void SetDevToolsAgentHost(scoped_refptr<content::DevToolsAgentHost> host);

 private:
  JavaObjectWeakGlobalRef weak_java_tab_;

  // Identifier of the window the tab is in.
  SessionID session_window_id_;

  scoped_refptr<cc::Layer> content_layer_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<android::TabWebContentsDelegateAndroid>
      web_contents_delegate_;
  scoped_refptr<content::DevToolsAgentHost> devtools_host_;
  std::unique_ptr<browser_sync::SyncedTabDelegateAndroid> synced_tab_delegate_;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
