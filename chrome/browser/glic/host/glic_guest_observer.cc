// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include "base/logging.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace glic {

WEB_CONTENTS_USER_DATA_KEY_IMPL(GlicGuestObserver);

GlicGuestObserver::GlicGuestObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<GlicGuestObserver>(*web_contents) {}

GlicGuestObserver::~GlicGuestObserver() = default;

void GlicGuestObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (IsGlicGuest(web_contents())) {
    render_frame_host->EnableMojoJsBindings(/*features=*/nullptr);
  }
}

void GlicGuestObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return;
  }
  auto* rfh = navigation_handle->GetRenderFrameHost();
  if (IsFrameAllowedGlicApi(*rfh)) {
    rfh->EnableMojoJsBindings(/*features=*/nullptr);
  }
}

}  // namespace glic
