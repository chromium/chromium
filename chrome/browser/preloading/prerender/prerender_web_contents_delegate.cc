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
