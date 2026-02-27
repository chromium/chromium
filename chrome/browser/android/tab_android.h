// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_ANDROID_H_

#include <jni.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/tab_android_data_provider.h"
#include "chrome/browser/tab/web_contents_state.h"
#include "components/sessions/core/session_id.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/token_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class GURL;
class TabAndroidDataProvider;
class Profile;

namespace cc::slim {
class Layer;
}  // namespace cc::slim

namespace android {
class TabWebContentsDelegateAndroid;
}  // namespace android

namespace browser_sync {
class SyncedTabDelegateAndroid;
}  // namespace browser_sync

namespace content {
class DevToolsAgentHost;
class WebContents;
}  // namespace content

namespace sync_sessions {
class SyncedTabDelegate;
}  // namespace sync_sessions

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
  // WebContents. Can return nullptr.
  static TabAndroid* FromWebContents(content::WebContents* web_contents);
  static const TabAndroid* FromWebContents(
      const content::WebContents* web_contents);

  // Returns the native TabAndroid associated with the given `handle`.
  // Returns nullptr if the `handle` is not associated with a TabAndroid.
  static TabAndroid* FromTabHandle(tabs::TabHandle handle);

  // Returns the native TabAndroid stored in the Java Tab represented by
  // |obj|.
  static TabAndroid* GetNativeTab(JNIEnv* env,
                                  const base::android::JavaRef<jobject>& obj);

  // Returns the a vector of native TabAndroid stored in the Java Tab array
  // represented by |obj_array|.
  static std::vector<raw_ptr<TabAndroid, VectorExperimental>> GetAllNativeTabs(
      JNIEnv* env,
      const base::android::ScopedJavaLocalRef<jobjectArray>& obj_array);

  // Function to attach helpers to the `web_contents`.
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
      const override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

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

  bool IsCustomTab() const;
  bool IsHidden() const;

  bool IsTrustedWebActivity() const;

  // Set the media state of the tab. This is called by MediaStateObserver.
  void SetMediaState(int media_state);

  // Observers -----------------------------------------------------------------

  // Adds/Removes an Observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods called from Java via JNI -----------------------------------------

  void Destroy();
  bool HasParentCollection();
  void InitWebContents(
      JNIEnv* env,
      bool incognito,
      bool is_background_tab,
      const base::android::JavaRef<jobject>& jweb_contents,
      const base::android::JavaRef<jobject>& jweb_contents_delegate,
      const base::android::JavaRef<jobject>& jcontext_menu_populator_factory);
  void InitializeAutofillIfNecessary();
  void UpdateDelegates(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jweb_contents_delegate,
      const base::android::JavaRef<jobject>& jcontext_menu_populator_factory);
  void SendDidActivateUpdate(JNIEnv* env);
  void SendWillDeactivateUpdate(JNIEnv* env);
  void SendWillDetachUpdate(JNIEnv* env, int32_t detach_reason);
  void SendDidInsertUpdate(JNIEnv* env);
  void DestroyWebContents();
  void ReleaseWebContents();
  bool IsPhysicalBackingSizeEmpty(
      const base::android::JavaRef<jobject>& jweb_contents);
  void OnPhysicalBackingSizeChanged(
      const base::android::JavaRef<jobject>& jweb_contents,
      int32_t width,
      int32_t height);
  void SetActiveNavigationEntryTitleForUrl(const std::string& jurl,
                                           const std::u16string& jtitle);
  void LoadOriginalImage();
  void OnShow();
  void NotifyPinnedStateChanged(bool is_pinned);
  void NotifyTabGroupChanged(std::optional<base::Token> tab_group_id);
  bool IsDragging() const;
  void OnDraggingStateChanged(bool is_dragging);
  using DraggingChangedCallback =
      base::RepeatingCallback<void(TabInterface*, bool)>;
  base::CallbackListSubscription RegisterDraggingChanged(
      DraggingChangedCallback callback);

  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost();

  void SetDevToolsAgentHost(scoped_refptr<content::DevToolsAgentHost> host);

  base::WeakPtr<TabAndroid> GetTabAndroidWeakPtr();

  // TabInterface overrides:
  base::WeakPtr<tabs::TabInterface> GetWeakPtr() override;
  content::WebContents* GetContents() const override;
  // This implementation of close immediately closes the tab without undo
  // support and without a warning dialog when closing the last tab in a tab
  // group. For more granular control it is strongly recommended to close tabs
  // from Java instead. This operation may fail if the TabModel for this tab is
  // not found for some reason.
  void Close() override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) override;
  bool IsActivated() const override;
  base::CallbackListSubscription RegisterDidActivate(
      DidActivateCallback callback) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      WillDeactivateCallback callback) override;
  bool IsVisible() const override;
  bool IsSelected() const override;
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
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  const BrowserWindowInterface* GetBrowserWindowInterface() const override;
  tabs::TabFeatures* GetTabFeatures() override;
  const tabs::TabFeatures* GetTabFeatures() const override;
  bool IsPinned() const override;
  bool IsBlocked() const override;
  bool IsSplit() const override;
  std::optional<tab_groups::TabGroupId> GetGroup() const override;
  std::optional<split_tabs::SplitTabId> GetSplit() const override;
  tabs::TabCollection* GetParentCollection(
      base::PassKey<tabs::TabCollection>) const override;
  const tabs::TabCollection* GetParentCollection() const override;

  void OnReparented(tabs::TabCollection* parent,
                    base::PassKey<tabs::TabCollection>) override;
  void OnAncestorChanged(base::PassKey<tabs::TabCollection>) override;

  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;

 private:
  // This constructor bypassing JVM setup is for CreateForTesting only.
  TabAndroid(Profile* profile, int tab_id);

  void UpdateProperties();
  void SetIsPinned(bool pinned);
  void SetIsDragging(bool dragging);
  void SetTabGroupId(std::optional<tab_groups::TabGroupId> tab_group_id);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env) const;

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

  raw_ptr<tabs::TabCollection> parent_collection_ = nullptr;

  base::ObserverList<Observer> observers_;

  base::RepeatingCallbackList<void(TabInterface*, bool)>
      pinned_state_changed_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*,
                                   std::optional<tab_groups::TabGroupId>)>
      group_changed_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*, bool)>
      dragging_changed_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*)> did_activate_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*)>
      will_deactivate_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*)>
      did_become_visible_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*)>
      will_become_hidden_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*,
                                   tabs::TabInterface::DetachReason)>
      will_detach_callback_list_;
  base::RepeatingCallbackList<void(TabInterface*)> did_insert_callback_list_;

  const base::WeakPtr<Profile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::WeakPtrFactory<TabAndroid> weak_ptr_factory_{this};
};

namespace jni_zero {
template <>
inline TabAndroid* FromJniType<TabAndroid*>(JNIEnv* env,
                                            const JavaRef<jobject>& j_object) {
  return j_object.is_null() ? nullptr : TabAndroid::GetNativeTab(env, j_object);
}
template <>
inline ScopedJavaLocalRef<jobject> ToJniType<TabAndroid>(
    JNIEnv* env,
    const TabAndroid& tab) {
  return tab.GetJavaObject();
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
