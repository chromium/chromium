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
#include "base/types/pass_key.h"
#include "chrome/browser/android/tab_android_data_provider.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/token_id.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"
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

namespace tabs {
class TabCollection;
class TabFeatures;
}  // namespace tabs

class TabAndroid : public tabs::TabInterface,
                   public TabAndroidDataProvider,
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

  static std::unique_ptr<TabAndroid> CreateForTesting(
      Profile* profile,
      int tab_id,
      std::unique_ptr<content::WebContents> web_contents);

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

  // Returns launch type at creation. May be TabLaunchType::UNSET if unknown.
  int GetTabLaunchTypeAtCreation() const;

  // Returns the parent tab identifier for the tab.
  int GetParentId() const;

  // Delete navigation entries matching predicate from frozen state.
  void DeleteFrozenNavigationEntries(
      const WebContentsState::DeletionPredicate& predicate);

  void SetWindowSessionID(SessionID window_id);

  std::unique_ptr<content::WebContents> SwapWebContents(
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load);

  bool IsCustomTab() const;
  bool IsHidden() const;

  bool IsTrustedWebActivity() const;

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
  bool IsPhysicalBackingSizeEmpty(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  void SetActiveNavigationEntryTitleForUrl(JNIEnv* env,
                                           std::string& jurl,
                                           std::u16string& jtitle);

  void LoadOriginalImage(JNIEnv* env);
  void OnShow(JNIEnv* env);

  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost();

  void SetDevToolsAgentHost(scoped_refptr<content::DevToolsAgentHost> host);

  base::WeakPtr<TabAndroid> GetTabAndroidWeakPtr();

  // TabInterface overrides:
  base::WeakPtr<tabs::TabInterface> GetWeakPtr() override;
  content::WebContents* GetContents() const override;
  void Close() override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) override;
  bool IsActivated() const override;
  base::CallbackListSubscription RegisterDidActivate(
      DidActivateCallback callback) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      WillDeactivateCallback callback) override;
  bool IsVisible() const override;
  base::CallbackListSubscription RegisterDidBecomeVisible(
      DidBecomeVisibleCallback callback) override;
  base::CallbackListSubscription RegisterWillBecomeHidden(
      WillBecomeHiddenCallback callback) override;
  base::CallbackListSubscription RegisterWillDetach(
      WillDetach callback) override;
  base::CallbackListSubscription RegisterDidInsert(
      DidInsertCallback callback) override;
  base::CallbackListSubscription RegisterPinnedStateChanged(
      PinnedStateChangedCallback callback) override;
  base::CallbackListSubscription RegisterGroupChanged(
      GroupChangedCallback callback) override;
  bool CanShowModalUI() const override;
  std::unique_ptr<tabs::ScopedTabModalUI> ShowModalUI() override;
  base::CallbackListSubscription RegisterModalUIChanged(
      TabInterfaceCallback callback) override;
  bool IsInNormalWindow() const override;
  tabs::TabFeatures* GetTabFeatures() override;
  const tabs::TabFeatures* GetTabFeatures() const override;
  bool IsPinned() const override;
  bool IsSplit() const override;
  std::optional<tab_groups::TabGroupId> GetGroup() const override;
  std::optional<split_tabs::SplitTabId> GetSplit() const override;
  tabs::TabCollection* GetParentCollection(
      base::PassKey<tabs::TabCollection>) const override;
  const tabs::TabCollection* GetParentCollection() const override;

  void OnReparented(tabs::TabCollection* parent,
                    base::PassKey<tabs::TabCollection>) override;
  void OnAncestorChanged(base::PassKey<tabs::TabCollection>) override;

 private:
  // This constructor bypassing JVM setup is for CreateForTesting only.
  TabAndroid(Profile* profile, int tab_id);
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

  // Holds tab-scoped state. Constructed after tab_helpers.
  std::unique_ptr<tabs::TabFeatures> tab_features_;

  base::ObserverList<Observer> observers_;

  const base::WeakPtr<Profile> profile_;
  base::WeakPtrFactory<TabAndroid> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
