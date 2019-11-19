// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "android_webview/browser/js_java_interaction/js_java_configurator_host.h"
#include "android_webview/browser/permission/permission_request_handler_client.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;

namespace autofill {
class AutofillProvider;
}

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
class AwContents : public FindHelper::Listener,
                   public IconHelper::Listener,
                   public AwRenderViewHostExtClient,
                   public BrowserViewRendererClient,
                   public PermissionRequestHandlerClient,
                   public AwBrowserPermissionRequestDelegate,
                   public AwRenderProcessGoneDelegate,
                   public content::WebContentsObserver,
                   public AwSafeBrowsingUIManager::UIManagerClient {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  static std::string GetLocale();

  static std::string GetLocaleList();

  AwContents(std::unique_ptr<content::WebContents> web_contents);
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
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& aw_contents,
      const base::android::JavaParamRef<jobject>& web_contents_delegate,
      const base::android::JavaParamRef<jobject>& contents_client_bridge,
      const base::android::JavaParamRef<jobject>& io_thread_client,
      const base::android::JavaParamRef<jobject>& intercept_navigation_delegate,
      const base::android::JavaParamRef<jobject>& autofill_provider);
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobject> GetBrowserContext(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetCompositorFrameConsumer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong compositor_frame_consumer);
  base::android::ScopedJavaLocalRef<jobject> GetRenderProcess(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void Destroy(JNIEnv* env);
  void DocumentHasImages(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jobject>& message);
  void GenerateMHTML(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& jpath,
                     const base::android::JavaParamRef<jobject>& callback);
  void CreatePdfExporter(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& pdfExporter);
  void AddVisitedLinks(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& jvisited_links);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RequestNewHitTestDataAt(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jfloat x,
                               jfloat y,
                               jfloat touch_major);
  void UpdateLastHitTestData(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void OnSizeChanged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     int w,
                     int h,
                     int ow,
                     int oh);
  void SetViewVisibility(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         bool visible);
  void SetWindowVisibility(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool visible);
  void SetIsPaused(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   bool paused);
  void OnAttachedToWindow(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          int w,
                          int h);
  void OnDetachedFromWindow(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  bool IsVisible(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetOpaqueState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean RestoreFromOpaqueState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& state);
  void FocusFirstNode(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void SetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint color);
  void ZoomBy(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jfloat delta);
  void OnComputeScroll(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jlong animation_time_millis);
  bool OnDraw(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              const base::android::JavaParamRef<jobject>& canvas,
              jboolean is_hardware_accelerated,
              jint scroll_x,
              jint scroll_y,
              jint visible_left,
              jint visible_top,
              jint visible_right,
              jint visible_bottom,
              jboolean force_auxiliary_bitmap_rendering);
  bool NeedToDrawBackgroundColor(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);
  jlong CapturePicture(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       int width,
                       int height);
  void EnableOnNewPicture(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean enabled);
  void InsertVisualStateCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong request_id,
      const base::android::JavaParamRef<jobject>& callback);
  void ClearView(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetExtraHeadersForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& extra_headers);

  void InvokeGeolocationCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean value,
      const base::android::JavaParamRef<jstring>& origin);

  jint GetEffectivePriority(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  JsJavaConfiguratorHost* GetJsJavaConfiguratorHost();

  base::android::ScopedJavaLocalRef<jstring> AddWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& listener,
      const base::android::JavaParamRef<jstring>& js_object_name,
      const base::android::JavaParamRef<jobjectArray>& allowed_origins);

  void RemoveWebMessageListener(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& js_object_name);

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
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& origin,
      jlong resources);

  // AwBrowserPermissionRequestDelegate implementation.
  void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override;
  void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) override;
  void RequestGeolocationPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override;
  void CancelGeolocationPermissionRequests(const GURL& origin) override;
  void RequestMIDISysexPermission(
      const GURL& origin,
      base::OnceCallback<void(bool)> callback) override;
  void CancelMIDISysexPermissionRequests(const GURL& origin) override;

  // Find-in-page API and related methods.
  void FindAllAsync(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& search_string);
  void FindNext(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean forward);
  void ClearMatches(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  FindHelper* GetFindHelper();

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
  void PostInvalidate() override;
  void OnNewPicture() override;
  gfx::Point GetLocationOnScreen() override;
  void OnViewTreeForceDarkStateChanged(
      bool view_tree_force_dark_state) override;

  // |new_value| is in physical pixel scale.
  void ScrollContainerViewTo(const gfx::Vector2d& new_value) override;

  void UpdateScrollState(const gfx::Vector2d& max_scroll_offset,
                         const gfx::SizeF& contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override;
  void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                     const gfx::Vector2dF& overscroll_velocity) override;
  ui::TouchHandleDrawable* CreateDrawable() override;

  void ClearCache(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jboolean include_disk_files);
  void KillRenderProcess(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void SetPendingWebContentsForPopup(
      std::unique_ptr<content::WebContents> pending);
  jlong ReleasePopupAwContents(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  void ScrollTo(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint x,
                jint y);
  void RestoreScrollAfterTransition(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint x,
      jint y);
  void SmoothScroll(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint target_x,
                    jint target_y,
                    jlong duration_ms);
  void SetDipScale(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jfloat dip_scale);
  void SetSaveFormData(bool enabled);

  // Sets the java client
  void SetAwAutofillClient(const base::android::JavaRef<jobject>& client);

  void SetJsOnlineProperty(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jboolean network_up);
  void TrimMemory(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint level,
                  jboolean visible);

  void GrantFileSchemeAccesstoChildProcess(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void ResumeLoadingCreatedPopupWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jlong GetAutofillProvider(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  void RendererUnresponsive(content::RenderProcessHost* render_process_host);
  void RendererResponsive(content::RenderProcessHost* render_process_host);

  // content::WebContentsObserver overrides
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;

  // AwSafeBrowsingUIManager::UIManagerClient implementation
  bool CanShowInterstitial() override;
  int GetErrorUiType() override;

  void EvaluateJavaScriptOnInterstitialForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& script,
      const base::android::JavaParamRef<jobject>& callback);

  // AwRenderProcessGoneDelegate overrides
  RenderProcessGoneResult OnRenderProcessGone(int child_process_id,
                                              bool crashed) override;

 private:
  void InitAutofillIfNecessary(bool autocomplete_enabled);

  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin,
                             base::OnceCallback<void(bool)>);
  void HideGeolocationPrompt(const GURL& origin);

  void SetDipScaleInternal(float dip_scale);

  JavaObjectWeakGlobalRef java_ref_;
  BrowserViewRenderer browser_view_renderer_;  // Must outlive |web_contents_|.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AwWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<AwContentsClientBridge> contents_client_bridge_;
  std::unique_ptr<AwRenderViewHostExt> render_view_host_ext_;
  std::unique_ptr<FindHelper> find_helper_;
  std::unique_ptr<IconHelper> icon_helper_;
  std::unique_ptr<AwContents> pending_contents_;
  std::unique_ptr<AwPdfExporter> pdf_exporter_;
  std::unique_ptr<PermissionRequestHandler> permission_request_handler_;
  std::unique_ptr<autofill::AutofillProvider> autofill_provider_;
  std::unique_ptr<JsJavaConfiguratorHost> js_java_configurator_host_;

  bool view_tree_force_dark_state_ = false;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, base::OnceCallback<void(bool)>> OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_H_
