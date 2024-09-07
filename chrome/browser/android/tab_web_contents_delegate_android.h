// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_

#include <memory>

#include "base/scoped_multi_source_observation.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"

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
      public find_in_page::FindResultObserver {
 public:
  TabWebContentsDelegateAndroid(JNIEnv* env,
                                const jni_zero::JavaRef<jobject>& obj);

  TabWebContentsDelegateAndroid(const TabWebContentsDelegateAndroid&) = delete;
  TabWebContentsDelegateAndroid& operator=(
      const TabWebContentsDelegateAndroid&) = delete;

  ~TabWebContentsDelegateAndroid() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void CreateSmsPrompt(content::RenderFrameHost*,
                       const std::vector<url::Origin>&,
                       const std::string& one_time_code,
                       base::OnceClosure on_confirm,
                       base::OnceClosure on_cancel) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
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
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  void SetOverlayMode(bool use_overlay_mode) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool ShouldResumeRequestsForCreatedWindow() override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void OnDidBlockNavigation(
      content::WebContents* web_contents,
      const GURL& blocked_url,
      const GURL& initiator_url,
      blink::mojom::NavigationBlockedReason reason) override;
  void UpdateUserGestureCarryoverInfo(
      content::WebContents* web_contents) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  bool IsBackForwardCacheSupported(content::WebContents& web_contents) override;
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  device::mojom::GeolocationContext* GetInstalledWebappGeolocationContext()
      override;

#if BUILDFLAG(ENABLE_PRINTING)
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  void CapturePaintPreviewOfSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      content::RenderFrameHost* render_frame_host) override;
#endif

  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;
  void OnFindTabHelperDestroyed(find_in_page::FindTabHelper* helper) override;

  bool ShouldEnableEmbeddedMediaExperience() const;
  bool IsPictureInPictureEnabled() const;
  bool IsNightModeEnabled() const;
  bool IsForceDarkWebContentEnabled() const;
  bool CanShowAppBanners() const;

  // Returns true if this tab is currently presented in the context of custom
  // tabs. Tabs can be moved between different activities so the returned value
  // might change over the lifetime of the tab.
  bool IsCustomTab() const;
  const GURL GetManifestScope() const;
  bool IsInstalledWebappDelegateGeolocation() const;
  bool IsModalContextMenu() const;

 private:
  std::unique_ptr<device::mojom::GeolocationContext>
      installed_webapp_geolocation_context_;

  base::ScopedMultiSourceObservation<find_in_page::FindTabHelper,
                                     find_in_page::FindResultObserver>
      find_result_observations_{this};
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_TAB_WEB_CONTENTS_DELEGATE_ANDROID_H_
