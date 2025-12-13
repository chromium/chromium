// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/prerender_web_contents_delegate.h"
#include "chrome/browser/ui/tab_helpers.h"

void PrerenderWebContentsDelegateImpl::PrerenderWebContentsCreated(
    content::WebContents* prerender_web_contents) {
  TabHelpers::AttachTabHelpers(prerender_web_contents);
}
