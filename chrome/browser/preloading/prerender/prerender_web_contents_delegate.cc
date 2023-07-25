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
