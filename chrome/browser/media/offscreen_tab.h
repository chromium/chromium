// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_OFFSCREEN_TAB_H_
#define CHROME_BROWSER_MEDIA_OFFSCREEN_TAB_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace media_router {
class NavigationPolicy;
}  // namespace media_router

namespace content {
class BrowserContext;
}  // namespace content

// Owns and controls a sandboxed WebContents instance hosting the rendering
// engine for an offscreen tab.  Since the offscreen tab does not interact with
// the user in any direct way, the WebContents is not attached to any Browser
// window/UI, and any input and focusing capabilities are blocked.
//
// An OffscreenTab instance is shut down one of the three ways:
//
//   1. When WebContents::IsBeingCaptured() returns false, indicating there are
//      no more consumers of its captured content (e.g., when all MediaStreams
//      have been closed).  OffscreenTab will auto-detect this case and
//      self-destruct.
//   2. By the renderer, where the WebContents implementation will invoke the
//      WebContentsDelegate::CloseContents() override.  This occurs, for
//      example, when a page calls window.close().
//   3. Automatically, when the associated profile is destroyed.
//
// This class operates exclusively on the UI thread and so is not thread-safe.
class OffscreenTab final : public ProfileObserver,
                           protected content::WebContentsDelegate,
                           protected content::WebContentsObserver {
 public:
  // TODO(crbug.com/40781745): Hold the OffscreenTab by a WeakPtr, then Owner
  // can be deleted.
  class Owner {
   public:
    virtual ~Owner() {}

    // |tab| is no longer valid after this call.
    virtual void DestroyTab(OffscreenTab* tab) = 0;
  };

  OffscreenTab(Owner* owner, content::BrowserContext* context);

  OffscreenTab(const OffscreenTab&) = delete;
  OffscreenTab& operator=(const OffscreenTab&) = delete;

  ~OffscreenTab() final;

  // The WebContents instance hosting the rendering engine for this
  // OffscreenTab.
  content::WebContents* web_contents() const {
    return offscreen_tab_web_contents_.get();
  }

  // Creates the WebContents instance containing the offscreen tab's page,
  // configures it for offscreen rendering at the given |initial_size|, and
  // navigates it to |start_url|.  This is invoked once after construction.
  void Start(const GURL& start_url,
             const gfx::Size& initial_size,
             const std::string& optional_presentation_id);

  // Closes the underlying WebContents.
  void Close();

 private:
#if defined(USE_AURA)
  class WindowAdoptionAgent;
#endif  // defined(USE_AURA)

  // content::WebContentsDelegate overrides to provide the desired behaviors.
  void CloseContents(content::WebContents* source) final;
  bool ShouldSuppressDialogs(content::WebContents* source) final;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) final;
  bool ShouldFocusPageAfterCrash(content::WebContents* source) final;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) final;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) final;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) final;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) final;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) final;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) final;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) final;
  void ExitFullscreenModeForTab(content::WebContents* contents) final;
  bool IsFullscreenForTabOrPending(const content::WebContents* contents) final;
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* contents) final;
  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) final;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) final;

  // content::WebContentsObserver overrides
  void DidStartNavigation(content::NavigationHandle* navigation_handle) final;

  bool in_fullscreen_mode() const { return !non_fullscreen_size_.IsEmpty(); }

  // Called by |capture_poll_timer_| to automatically destroy this OffscreenTab
  // when the capturer count returns to zero.
  void DieIfContentCaptureEnded();

  // Called if OTR profile is being destroyed and |this| therefore needs to be
  // destroyed also.
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  const raw_ptr<Owner> owner_;  // Outlives this class.

  // The initial navigation URL, which may or may not match the current URL if
  // page-initiated navigations have occurred.
  GURL start_url_;

  // A non-shared off-the-record profile based on the profile of the extension
  // background page.
  raw_ptr<Profile> otr_profile_;

  // The WebContents containing the off-screen tab's page.
  std::unique_ptr<content::WebContents> offscreen_tab_web_contents_;

  // The time at which Start() finished creating |offscreen_tab_web_contents_|.
  base::TimeTicks start_time_;

  // Set to the original size of the renderer just before entering fullscreen
  // mode.  When not in fullscreen mode, this is an empty size.
  gfx::Size non_fullscreen_size_;

  // Poll timer to monitor the capturer count on |offscreen_tab_web_contents_|.
  // When the capturer count returns to zero, this OffscreenTab is automatically
  // destroyed.
  // TODO(crbug.com/41207731): add a method to WebContentsObserver to
  // report capturer count and get rid of this polling-based approach.
  base::OneShotTimer capture_poll_timer_;

  // This is false until after the Start() method is called, and capture of the
  // |offscreen_tab_web_contents_| is first detected.
  bool content_capture_was_detected_;

  // Object consulted to determine which offscreen tab navigations are allowed.
  std::unique_ptr<media_router::NavigationPolicy> navigation_policy_;

#if defined(USE_AURA)
  std::unique_ptr<WindowAdoptionAgent> window_agent_;
#endif  // defined(USE_AURA)
};

#endif  // CHROME_BROWSER_MEDIA_OFFSCREEN_TAB_H_
