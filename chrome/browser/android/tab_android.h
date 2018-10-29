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
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/tab_state.h"
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/platform/media_download_in_product_help.mojom.h"

class GURL;
class Profile;

namespace cc {
class Layer;
}

struct NavigateParams;

namespace android {
class TabWebContentsDelegateAndroid;
class TabContentManager;
}

namespace content {
class DevToolsAgentHost;
class NavigationHandle;
class WebContents;
}

namespace prerender {
class PrerenderManager;
}

class TabAndroid : public CoreTabHelperDelegate,
                   public content::NotificationObserver,
                   public favicon::FaviconDriverObserver,
                   public content::WebContentsObserver {
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
  ~TabAndroid() override;

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

  // Load the tab if it was unloaded from memory.
  bool LoadIfNeeded();

  // Helper methods to make it easier to access objects from the associated
  // WebContents.  Can return NULL.
  Profile* GetProfile() const;
  sync_sessions::SyncedTabDelegate* GetSyncedTabDelegate() const;

  // Delete navigation entries matching predicate from frozen state.
  void DeleteFrozenNavigationEntries(
      const WebContentsState::DeletionPredicate& predicate);

  void SetWindowSessionID(SessionID window_id);
  void SetSyncId(int sync_id);

  void HandlePopupNavigation(NavigateParams* params);

  bool HasPrerenderedUrl(GURL gurl);

  // Overridden from NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Overridden from favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // Returns true if this tab is currently presented in the context of custom
  // tabs. Tabs can be moved between different activities so the returned value
  // might change over the lifetime of the tab.
  bool IsCurrentlyACustomTab();

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
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean delete_native);
  void OnPhysicalBackingSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents,
      jint width,
      jint height);
  base::android::ScopedJavaLocalRef<jobject> GetProfileAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  TabLoadStatus LoadUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& j_extra_headers,
      const base::android::JavaParamRef<jobject>& j_post_data,
      jint page_transition,
      const base::android::JavaParamRef<jstring>& j_referrer_url,
      jint referrer_policy,
      jboolean is_renderer_initiated,
      jboolean should_replace_current_entry,
      jboolean has_user_gesture,
      jboolean should_clear_history_list,
      jlong omnibox_input_received_timestamp);
  void SetActiveNavigationEntryTitleForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      const base::android::JavaParamRef<jstring>& jtitle);
  bool Print(JNIEnv* env,
             const base::android::JavaParamRef<jobject>& obj,
             jint render_process_id,
             jint render_frame_id);

  // Sets the tab as content to be printed through JNI.
  void SetPendingPrint(int render_process_id, int render_frame_id);

  // Called to get default favicon of current tab, return null if no
  // favicon is avaliable for current tab.
  base::android::ScopedJavaLocalRef<jobject> GetFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void CreateHistoricalTab(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  static void CreateHistoricalTabFromContents(
      content::WebContents* web_contents);

  void UpdateBrowserControlsState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint constraints,
      jint current,
      jboolean animate);

  void LoadOriginalImage(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  jlong GetBookmarkId(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jboolean only_editable);

  void SetInterceptNavigationDelegate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& delegate);

  // TODO(dtrainor): Remove this, pull content_layer() on demand.
  void AttachToTabContentManager(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jtab_content_manager);

  void ClearThumbnailPlaceholder(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jint GetCurrentRenderProcessId(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  bool HasPrerenderedUrl(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& url);

  void SetWebappManifestScope(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& scope);

  const GURL& GetWebappManifestScope() const { return webapp_manifest_scope_; }

  void SetPictureInPictureEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);

  bool IsPictureInPictureEnabled() const;

  void EnableEmbeddedMediaExperience(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);

  void MediaDownloadInProductHelpDismissed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  bool ShouldEnableEmbeddedMediaExperience() const;

  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost();

  void SetDevToolsAgentHost(scoped_refptr<content::DevToolsAgentHost> host);

  void AttachDetachedTab(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  // Register the Tab's native methods through JNI.
  static bool RegisterTabAndroid(JNIEnv* env);

  // content::WebContentsObserver implementation.
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void NavigationEntryChanged(
      const content::EntryChangedDetails& change_details) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  bool AreRendererInputEventsIgnored(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  class MediaDownloadInProductHelp;

  prerender::PrerenderManager* GetPrerenderManager() const;

  // methods used by MediaDownloadInProductHelp.
  void CreateInProductHelpService(
      blink::mojom::MediaDownloadInProductHelpRequest request,
      content::RenderFrameHost* render_frame_host);
  void ShowMediaDownloadInProductHelp(const gfx::Rect& rect_in_frame);
  void DismissMediaDownloadInProductHelp();
  void OnMediaDownloadInProductHelpConnectionError();

  JavaObjectWeakGlobalRef weak_java_tab_;

  // Identifier of the window the tab is in.
  SessionID session_window_id_;

  content::NotificationRegistrar notification_registrar_;

  scoped_refptr<cc::Layer> content_layer_;
  android::TabContentManager* tab_content_manager_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<android::TabWebContentsDelegateAndroid>
      web_contents_delegate_;
  scoped_refptr<content::DevToolsAgentHost> devtools_host_;
  std::unique_ptr<browser_sync::SyncedTabDelegateAndroid> synced_tab_delegate_;

  GURL webapp_manifest_scope_;
  bool picture_in_picture_enabled_;
  bool embedded_media_experience_enabled_;

  std::unique_ptr<MediaDownloadInProductHelp> media_in_product_help_;

  service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>
      frame_interfaces_;

  base::WeakPtrFactory<TabAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_TAB_ANDROID_H_
