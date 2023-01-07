// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_H_

#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl_delegate.h"

namespace content {
class BrowserContext;
}

namespace prerender {

class NoStatePrefetchLinkManager;

class ChromeNoStatePrefetchProcessorImplDelegate
    : public NoStatePrefetchProcessorImplDelegate {
 public:
  ChromeNoStatePrefetchProcessorImplDelegate() = default;
  ~ChromeNoStatePrefetchProcessorImplDelegate() override = default;

  // NoStatePrefetchProcessorImplDelegate overrides,
  NoStatePrefetchLinkManager* GetNoStatePrefetchLinkManager(
      content::BrowserContext* browser_context) override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_PROCESSOR_IMPL_DELEGATE_H_
