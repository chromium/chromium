// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_WEB_CONTENTS_DELEGATE_H_

#include "content/public/browser/prerender_web_contents_delegate.h"

// This is an implementation of content::PrerenderWebContentsDelegate. See
// comments on content::PrerenderWebContentsDelegate for details.
class PrerenderWebContentsDelegateImpl
    : public content::PrerenderWebContentsDelegate {
 public:
  PrerenderWebContentsDelegateImpl() = default;
  ~PrerenderWebContentsDelegateImpl() override = default;

  // content::WebContentsDelegate overrides.
  content::PreloadingEligibility IsPrerender2Supported(
      content::WebContents& web_contents) override;
  // TODO(crbug.com/1350676): Investigate if we have to override other
  // functions on WebContentsDelegate.
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PRERENDER_WEB_CONTENTS_DELEGATE_H_
