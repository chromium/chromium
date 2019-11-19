// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/find_bar/find_result_observer.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/frame/blocked_navigation_types.h"

namespace content {
struct FileChooserParams;
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace url {
class Origin;
}

namespace android {

// Chromium Android specific WebContentsDelegate.
// Should contain any WebContentsDelegate implementations required by
// the Chromium Android port but not to be shared with WebView.
class TabWebContentsDelegateAndroid
    : public web_contents_delegate_android::WebContentsDelegateAndroid,
      public FindResultObserver {
 public:
  TabWebContentsDelegateAndroid(JNIEnv* env, jobject obj);
  ~TabWebContentsDelegateAndroid() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const url::Origin&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
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
                                  blink::mojom::MediaStreamType type) override;
  void SetOverlayMode(bool use_overlay_mode) override;
  void RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      base::OnceCallback<void(bool)> callback) override;
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
  blink::SecurityStyle GetSecurityStyle(
      content::WebContents* web_contents,
      content::SecurityStyleExplanations* security_style_explanations) override;
  void OnDidBlockNavigation(content::WebContents* web_contents,
                            const GURL& blocked_url,
                            const GURL& initiator_url,
                            blink::NavigationBlockedReason reason) override;
  void UpdateUserGestureCarryoverInfo(
      content::WebContents* web_contents) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents,
      const viz::SurfaceId&,
      const gfx::Size&) override;
  void ExitPictureInPicture() override;
  std::unique_ptr<content::WebContents> SwapWebContents(
      content::WebContents* old_contents,
      std::unique_ptr<content::WebContents> new_contents,
      bool did_start_load,
      bool did_finish_load) override;

#if BUILDFLAG(ENABLE_PRINTING)
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
#endif

  // FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;
  void OnFindTabHelperDestroyed(FindTabHelper* helper) override;

  bool ShouldEnableEmbeddedMediaExperience() const;
  bool IsPictureInPictureEnabled() const;
  bool IsNightModeEnabled() const;
  bool CanShowAppBanners() const;

  // Returns true if this tab is currently presented in the context of custom
  // tabs. Tabs can be moved between different activities so the returned value
  // might change over the lifetime of the tab.
  bool IsCustomTab() const;
  const GURL GetManifestScope() const;

 private:
  ScopedObserver<FindTabHelper, FindResultObserver> find_result_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(TabWebContentsDelegateAndroid);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
