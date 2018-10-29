// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include "base/files/file_path.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class FindNotificationDetails;

namespace content {
struct FileChooserParams;
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace android {

// Chromium Android specific WebContentsDelegate.
// Should contain any WebContentsDelegate implementations required by
// the Chromium Android port but not to be shared with WebView.
class TabWebContentsDelegateAndroid
    : public web_contents_delegate_android::WebContentsDelegateAndroid,
      public content::NotificationObserver {
 public:
  TabWebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  ~TabWebContentsDelegateAndroid() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  void CloseContents(content::WebContents* web_contents) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  blink::WebDisplayMode GetDisplayMode(
      const content::WebContents* web_contents) const override;
  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  void FindMatchRectsReply(content::WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  void AdjustPreviewsStateForNavigation(
      content::WebContents* web_contents,
      content::PreviewsState* previews_state) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;
  void SetOverlayMode(bool use_overlay_mode) override;
  bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool ShouldResumeRequestsForCreatedWindow() override;
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  blink::WebSecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations) override;
  void RequestAppBannerFromDevTools(
      content::WebContents* web_contents) override;
  void OnDidBlockFramebust(content::WebContents* web_contents,
                           const GURL& url) override;
  void UpdateUserGestureCarryoverInfo(
      content::WebContents* web_contents) override;
  std::unique_ptr<content::WebContents> SwapWebContents(
      content::WebContents* old_contents,
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load) override;

 private:
  // NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void OnFindResultAvailable(content::WebContents* web_contents,
                             const FindNotificationDetails* find_result);

  content::NotificationRegistrar notification_registrar_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
