// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_
#define CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_

#include "components/prerender/browser/prerender_processor_impl_delegate.h"

namespace content {
class BrowserContext;
}

namespace prerender {

class PrerenderLinkManager;

class ChromePrerenderProcessorImplDelegate
    : public PrerenderProcessorImplDelegate {
 public:
  ChromePrerenderProcessorImplDelegate() = default;
  ~ChromePrerenderProcessorImplDelegate() override = default;

  // PrerenderProcessorImplDelegate overrides,
  PrerenderLinkManager* GetPrerenderLinkManager(
      content::BrowserContext* browser_context) override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_CHROME_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_
