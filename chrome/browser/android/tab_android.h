// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "chrome/browser/android/tab_android_data_provider.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sessions/core/session_id.h"
#include "tab_android_data_provider.h"

class GURL;
class Profile;

namespace cc::slim {
class Layer;
}

namespace android {
class TabWebContentsDelegateAndroid;
}

namespace content {
class DevToolsAgentHost;
class WebContents;
}  // namespace content

class TabAndroid : public TabAndroidDataProvider,
                   public base::SupportsUserData {
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
  static std::vector<raw_ptr<TabAndroid, VectorExperimental>> GetAllNativeTabs(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jobjectArray>& obj_array);

  // Function to attach helpers to the contentView.
  static void AttachTabHelpers(content::WebContents* web_contents);

  TabAndroid(JNIEnv* env,
             const base::android::JavaRef<jobject>& obj,
             Profile* profile,
             int tab_id);

  TabAndroid(const TabAndroid&) = delete;
  TabAndroid& operator=(const TabAndroid&) = delete;

  ~TabAndroid() override;

  // TabAndroidDataProvider
  SessionID GetWindowId() const override;
  int GetAndroidId() const override;
  std::unique_ptr<WebContentsStateByteBuffer> GetWebContentsByteBuffer()
      override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Return the WebContents, if any, currently owned by this TabAndroid.
  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Return the cc::slim::Layer that represents the content for this TabAndroid.
  scoped_refptr<cc::slim::Layer> GetContentLayer() const;

  // Return the Profile* associated with this TabAndroid instance, or null, if
  // the profile no longer exists.
  // Note that this function should never return null in healthy situations.
  // Tabs are associated with a profile. Lack of valid profile indicates that
  // the Tab object held by caller is likely also not valid.
  Profile* profile() const { return profile_.get(); }

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

  sync_sessions::SyncedTabDelegate* GetSyncedTabDelegate() const;

  // Whether this tab is an incognito tab. Prefer
  // `profile()->IsOffTheRecord()` unless `web_contents()` is nullptr.
  bool IsIncognito() const;

  // Returns the time at which the tab was last shown to the user. Note that the
  // timestamp is when the tab comes into view, not the time it went out of
  // view.
  base::Time GetLastShownTimestamp() const;

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

  bool IsTrustedWebActivity();

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
  void InitializeAutofillIfNecessary(JNIEnv* env);
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

  base::WeakPtr<TabAndroid> GetWeakPtr();

 private:
  JavaObjectWeakGlobalRef weak_java_tab_;

  int tab_id_;

  // Identifier of the window the tab is in.
  SessionID session_window_id_;

  scoped_refptr<cc::slim::Layer> content_layer_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<android::TabWebContentsDelegateAndroid>
      web_contents_delegate_;
  scoped_refptr<content::DevToolsAgentHost> devtools_host_;
  std::unique_ptr<browser_sync::SyncedTabDelegateAndroid> synced_tab_delegate_;
  base::ObserverList<Observer> observers_;

  const base::WeakPtr<Profile> profile_;
  base::WeakPtrFactory<TabAndroid> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
