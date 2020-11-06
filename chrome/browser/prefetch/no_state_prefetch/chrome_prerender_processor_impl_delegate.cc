// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/no_state_prefetch/chrome_prerender_processor_impl_delegate.h"

#include "chrome/browser/prefetch/no_state_prefetch/prerender_link_manager_factory.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"
#include "content/public/browser/browser_context.h"

namespace prerender {

PrerenderLinkManager*
ChromePrerenderProcessorImplDelegate::GetPrerenderLinkManager(
    content::BrowserContext* browser_context) {
  return PrerenderLinkManagerFactory::GetForBrowserContext(browser_context);
}

}  // namespace prerender
