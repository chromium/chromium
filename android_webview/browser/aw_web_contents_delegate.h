// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_DELEGATE_H_

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

namespace android_webview {

// WebView specific WebContentsDelegate.
// Should contain WebContentsDelegate code required by WebView that should not
// be part of the Chromium Android port.
class AwWebContentsDelegate
    : public web_contents_delegate_android::WebContentsDelegateAndroid {
 public:
  AwWebContentsDelegate(JNIEnv* env, const jni_zero::JavaRef<jobject>& obj);
  ~AwWebContentsDelegate() override;

  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override;

  void RendererResponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host) override;

  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  bool UseFileChooserForFileSystemAccess() const override;
  // See //android_webview/docs/how-does-on-create-window-work.md for more
  // details.
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;

  void CloseContents(content::WebContents* source) override;
  void ActivateContents(content::WebContents* contents) override;
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  bool ShouldResumeRequestsForCreatedWindow() override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override;
  void UpdateUserGestureCarryoverInfo(
      content::WebContents* web_contents) override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  content::NavigationController::UserAgentOverrideOption
  ShouldOverrideUserAgentForPrerender2() override;
  bool ShouldAllowPartialParamMismatchOfPrerender2(
      content::NavigationHandle& navigation_handle) override;

  scoped_refptr<content::FileSelectListener> TakeFileSelectListener();

 private:
  bool is_fullscreen_;

  // Maintain a FileSelectListener instance passed to RunFileChooser() until
  // a callback is called.
  scoped_refptr<content::FileSelectListener> file_select_listener_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_DELEGATE_H_
