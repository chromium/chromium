// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_H_

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/gfx/browser_view_renderer.h"
#include "android_webview/browser/gfx/browser_view_renderer_client.h"
#include "android_webview/browser/icon_helper.h"
#include "android_webview/browser/metrics/visibility_metrics_logger.h"
#include "android_webview/browser/permission/permission_callback.h"
#include "android_webview/browser/permission/permission_request_handler_client.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_allowlist_manager.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_relationship_verification/digital_asset_links_handler.h"
#include "components/js_injection/browser/js_communication_host.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsClientBridge;
class AwPdfExporter;
class AwWebContentsDelegate;
class PermissionRequestHandler;

// Native side of java-class of same name.
//
// Object lifetime:
// For most purposes the java and native objects can be considered to have
// 1:1 lifetime and relationship. The exception is the java instance that
// hosts a popup will be rebound to a second native instance (carrying the
// popup content) and discard the 'default' native instance it made on
// construction. A native instance is only bound to at most one Java peer over
// its entire lifetime - see Init() and SetPendingWebContentsForPopup() for the
// construction points, and SetJavaPeers() where these paths join.
//
// Lifetime: WebView
class AwContents : public FindHelper::Listener,
                   public IconHelper::Listener,
                   public AwRenderViewHostExtClient,
                   public BrowserViewRendererClient,
                   public PermissionRequestHandlerClient,
                   public AwBrowserPermissionRequestDelegate,
                   public AwRenderProcessGoneDelegate,
                   public content::WebContentsObserver,
                   public AwSafeBrowsingUIManager::UIManagerClient,
                   public VisibilityMetricsLogger::Client,
                   public AwSafeBrowsingAllowlistSetObserver {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  static std::string GetLocale();

  static std::string GetLocaleList();

  AwContents(std::unique_ptr<content::WebContents> web_contents);

  AwContents(const AwContents&) = delete;
  AwContents& operator=(const AwContents&) = delete;

  ~AwContents() override;

  AwRenderViewHostExt* render_view_host_ext() {
    return render_view_host_ext_.get();
  }

  // |handler| is an instance of
  // org.chromium.android_webview.AwHttpAuthHandler.
  bool OnReceivedHttpAuthRequest(const base::android::JavaRef<jobject>& handler,
                                 const std::string& host,
                                 const std::string& realm);

  void SetOffscreenPreRaster(bool enabled);

  // Methods called from Java.
  void SetJavaPeers(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& aw_contents,
      const base::android::JavaParamRef<jobject>& web_contents_delegate,
      const base::android::JavaParamRef<jobject>& contents_client_bridge,
      const base::android::JavaParamRef<jobject>& io_thread_client,
      const base::android::JavaParamRef<jobject>&
          intercept_navigation_delegate);
  void InitializeAndroidAutofill(JNIEnv* env);
  void InitSensitiveContentClient(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetBrowserContext(JNIEnv* env);
  void SetCompositorFrameConsumer(JNIEnv* env, jlong compositor_frame_consumer);
  base::android::ScopedJavaLocalRef<jobject> GetRenderProcess(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();
  void Destroy(JNIEnv* env);
  void DocumentHasImages(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& message);
  void GenerateMHTML(JNIEnv* env,
                     const base::android::JavaParamRef<jstring>& jpath,
                     const base::android::JavaParamRef<jobject>& callback);
  void CreatePdfExporter(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& pdfExporter);
  void AddVisitedLinks(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jvisited_links);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(JNIEnv* env);
  void RequestNewHitTestDataAt(JNIEnv* env,
                               jfloat x,
                               jfloat y,
                               jfloat touch_major);
  void UpdateLastHitTestData(JNIEnv* env);
  void OnSizeChanged(JNIEnv* env, int w, int h, int ow, int oh);
  void OnConfigurationChanged(JNIEnv* env);
  void SetViewVisibility(JNIEnv* env, bool visible);
  void SetWindowVisibility(JNIEnv* env, bool visible);
  void SetIsPaused(JNIEnv* env, bool paused);
  void OnAttachedToWindow(JNIEnv* env, int w, int h);
  void OnDetachedFromWindow(JNIEnv* env);
  bool IsVisible(JNIEnv* env);
  bool IsDisplayingInterstitialForTesting(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jbyteArray> GetOpaqueState(JNIEnv* env);
  jboolean RestoreFromOpaqueState(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& state);
  void FocusFirstNode(JNIEnv* env);
  void SetBackgroundColor(JNIEnv* env, jint color);
  void ZoomBy(JNIEnv* env, jfloat delta);
  void OnComputeScroll(JNIEnv* env, jlong animation_time_millis);
  bool OnDraw(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& canvas,
              jboolean is_hardware_accelerated,
              jint scroll_x,
              jint scroll_y,
              jint visible_left,
              jint visible_top,
              jint visible_right,
              jint visible_bottom,
              jboolean force_auxiliary_bitmap_rendering);
  jfloat GetVelocityInPixelsPerSecond(JNIEnv* env);
  bool NeedToDrawBackgroundColor(JNIEnv* env);
  jlong CapturePicture(JNIEnv* env, int width, int height);
  void EnableOnNewPicture(JNIEnv* env, jboolean enabled);
  void InsertVisualStateCallback(
      JNIEnv* env,
      jlong request_id,
      const base::android::JavaParamRef<jobject>& callback);
  void ClearView(JNIEnv* env);
  void SetExtraHeadersForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& extra_headers);

  void InvokeGeolocationCallback(
      JNIEnv* env,
      jboolean value,
      const base::android::JavaParamRef<jstring>& origin);

  jint GetEffectivePriority(JNIEnv* env);

  js_injection::JsCommunicationHost* GetJsCommunicationHost();

  jint AddDocumentStartJavaScript(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobjectArray>& allowed_origin_rules);

  void RemoveDocumentStartJavaScript(JNIEnv* env, jint script_id);

  base::android::ScopedJavaLocalRef<jstring> AddWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& listener,
      const base::android::JavaParamRef<jstring>& js_object_name,
      const base::android::JavaParamRef<jobjectArray>& allowed_origins);

  void RemoveWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& js_object_name);

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> GetWebMessageListenerInfos(
      JNIEnv* env);

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
  GetDocumentStartupJavascripts(JNIEnv* env);

  void FlushBackForwardCache(JNIEnv* env, jint reason);

  void CancelAllPrerendering(JNIEnv* env);

  bool GetViewTreeForceDarkState() { return view_tree_force_dark_state_; }

  // PermissionRequestHandlerClient implementation.
  void OnPermissionRequest(base::android::ScopedJavaLocalRef<jobject> j_request,
                           AwPermissionRequest* request) override;
  void OnPermissionRequestCanceled(AwPermissionRequest* request) override;

  PermissionRequestHandler* GetPermissionRequestHandler() {
    return permission_request_handler_.get();
  }

  void PreauthorizePermission(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& origin,
      jlong resources);

  // AwBrowserPermissionRequestDelegate implementation.
  void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      PermissionCallback callback) override;
  void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) override;
  void RequestGeolocationPermission(const GURL& origin,
                                    PermissionCallback callback) override;
  void CancelGeolocationPermissionRequests(const GURL& origin) override;
  void RequestMIDISysexPermission(const GURL& origin,
                                  PermissionCallback callback) override;
  void CancelMIDISysexPermissionRequests(const GURL& origin) override;
  void RequestStorageAccess(const url::Origin& top_level_origin,
                            PermissionCallback callback) override;

  // Find-in-page API and related methods.
  void FindAllAsync(JNIEnv* env,
                    const base::android::JavaParamRef<jstring>& search_string);
  void FindNext(JNIEnv* env, jboolean forward);
  void ClearMatches(JNIEnv* env);
  FindHelper* GetFindHelper();

  // Per WebView Javascript Policy
  bool IsJavaScriptAllowed();

  // Per WebView Cookie Policy
  bool AllowThirdPartyCookies();

  // FindHelper::Listener implementation.
  void OnFindResultReceived(int active_ordinal,
                            int match_count,
                            bool finished) override;
  // IconHelper::Listener implementation.
  bool ShouldDownloadFavicon(const GURL& icon_url) override;
  void OnReceivedIcon(const GURL& icon_url, const SkBitmap& bitmap) override;
  void OnReceivedTouchIconUrl(const std::string& url,
                              const bool precomposed) override;

  // AwRenderViewHostExtClient implementation.
  void OnWebLayoutPageScaleFactorChanged(float page_scale_factor) override;
  void OnWebLayoutContentsSizeChanged(const gfx::Size& contents_size) override;

  // BrowserViewRendererClient implementation.
  void PostInvalidate(bool inside_vsync) override;
  void OnNewPicture() override;
  gfx::Point GetLocationOnScreen() override;
  void OnViewTreeForceDarkStateChanged(
      bool view_tree_force_dark_state) override;
  void SetPreferredFrameInterval(
      base::TimeDelta preferred_frame_interval) override;

  // |new_value| is in physical pixel scale.
  void ScrollContainerViewTo(const gfx::Point& new_value) override;

  void UpdateScrollState(const gfx::Point& max_scroll_offset,
                         const gfx::SizeF& contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override;
  void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                     const gfx::Vector2dF& overscroll_velocity,
                     bool inside_vsync) override;
  ui::TouchHandleDrawable* CreateDrawable() override;

  void ClearCache(JNIEnv* env, jboolean include_disk_files);
  // See //android_webview/docs/how-does-on-create-window-work.md for more
  // details.
  void SetPendingWebContentsForPopup(
      std::unique_ptr<content::WebContents> pending);
  jlong ReleasePopupAwContents(JNIEnv* env);

  void ScrollTo(JNIEnv* env, jint x, jint y);
  void RestoreScrollAfterTransition(JNIEnv* env, jint x, jint y);
  void SmoothScroll(JNIEnv* env,
                    jint target_x,
                    jint target_y,
                    jlong duration_ms);
  void SetDipScale(JNIEnv* env, jfloat dip_scale);
  base::android::ScopedJavaLocalRef<jstring> GetScheme(JNIEnv* env);
  void OnInputEvent(JNIEnv* env);

  void SetJsOnlineProperty(JNIEnv* env, jboolean network_up);
  void TrimMemory(JNIEnv* env, jint level, jboolean visible);

  void GrantFileSchemeAccesstoChildProcess(JNIEnv* env);

  void ResumeLoadingCreatedPopupWebContents(JNIEnv* env);

  void RendererUnresponsive(content::RenderProcessHost* render_process_host);
  void RendererResponsive(content::RenderProcessHost* render_process_host);

  bool UseLegacyGeolocationPermissionAPI();

  // content::WebContentsObserver overrides
  void PrimaryPageChanged(content::Page& page) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void RenderViewReady() override;

  // AwSafeBrowsingUIManager::UIManagerClient implementation
  bool CanShowInterstitial() override;
  int GetErrorUiType() override;

  // VisibilityMetricsLogger::Client implementation
  VisibilityMetricsLogger::VisibilityInfo GetVisibilityInfo() override;

  // AwRenderProcessGoneDelegate overrides
  RenderProcessGoneResult OnRenderProcessGone(int child_process_id,
                                              bool crashed) override;

  // AwSafeBrowsingAllowlistSetObserver overrides
  void OnSafeBrowsingAllowListSet() override;

 private:
  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin, PermissionCallback);
  void HideGeolocationPrompt(const GURL& origin);

  void SetDipScaleInternal(float dip_scale);

  // Callback for RequestStorageAccess to continue once the app_domain_list has
  // been loaded.
  void GrantRequestStorageAccessIfOriginIsAppDefined(
      const url::Origin top_level_origin,
      base::TimeTicks time_requested,
      PermissionCallback callback,
      bool is_app_defined);

  JavaObjectWeakGlobalRef java_ref_;
  BrowserViewRenderer browser_view_renderer_;  // Must outlive |web_contents_|.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AwWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<AwContentsClientBridge> contents_client_bridge_;
  std::unique_ptr<AwRenderViewHostExt> render_view_host_ext_;
  std::unique_ptr<FindHelper> find_helper_;
  std::unique_ptr<IconHelper> icon_helper_;
  // See //android_webview/docs/how-does-on-create-window-work.md for more
  // details for |pending_contents_|.
  std::unique_ptr<AwContents> pending_contents_;
  std::unique_ptr<AwPdfExporter> pdf_exporter_;
  std::unique_ptr<PermissionRequestHandler> permission_request_handler_;
  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;
  scoped_refptr<network::SharedURLLoaderFactory>
      storage_access_url_loader_factory_;
  std::unique_ptr<content_relationship_verification::DigitalAssetLinksHandler>
      asset_link_handler_;

  bool view_tree_force_dark_state_ = false;
  std::string scheme_;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, PermissionCallback> OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  base::TimeDelta preferred_frame_interval_;

  base::WeakPtrFactory<AwContents> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_H_
