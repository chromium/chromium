// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_CONTENTS_DELEGATE_H_

#include "components/prerender/browser/prerender_contents_delegate.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {
class WebContents;
}

namespace prerender {

class PrerenderContents;

class ChromePrerenderContentsDelegate : public PrerenderContentsDelegate {
 public:
  // Returns a PrerenderContents from the given web_contents, if it's used for
  // prerendering. Otherwise returns nullptr. Handles a nullptr input for
  // convenience.
  static PrerenderContents* FromWebContents(content::WebContents* web_contents);

  ChromePrerenderContentsDelegate() = default;
  ~ChromePrerenderContentsDelegate() override = default;

  // PrerenderContentsDelegate overrides.
  void OnPrerenderContentsCreated(content::WebContents* web_contents) override;
  void ReleasePrerenderContents(content::WebContents* web_contents) override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_CONTENTS_DELEGATE_H_
