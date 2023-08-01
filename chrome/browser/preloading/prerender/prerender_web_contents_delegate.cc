// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_web_contents_delegate.h"

content::PreloadingEligibility
PrerenderWebContentsDelegateImpl::IsPrerender2Supported(
    content::WebContents& web_contents) {
  // This should be checked in the initiator's WebContents.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // A prerendered page cannot open a new window.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::ActivateContents(
    content::WebContents* contents) {
  // WebContents should not be activated with this delegate.
  NOTREACHED_NORETURN();
}

bool PrerenderWebContentsDelegateImpl::ShouldSuppressDialogs(
    content::WebContents* source) {
  // Dialogs (JS dialogs and BeforeUnload confirm) should not be shown on a
  // prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  // A prerendered page should not create a new WebContents.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::PortalWebContentsCreated(
    content::WebContents* portal_web_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::WebContentsBecamePortal(
    content::WebContents* portal_web_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  // DCHECK against LifecycleState in RenderFrameHostImpl::DidBlockNavigation()
  // ensures this is never called during prerendering.
  NOTREACHED_NORETURN();
}

std::unique_ptr<content::WebContents>
PrerenderWebContentsDelegateImpl::ActivatePortalWebContents(
    content::WebContents* predecessor_contents,
    std::unique_ptr<content::WebContents> portal_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegateImpl::UpdateInspectedWebContentsIfNecessary(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    base::OnceCallback<void()> callback) {
  // This is called only for Portal that is not available on a prerendered page.
  NOTREACHED_NORETURN();
}
