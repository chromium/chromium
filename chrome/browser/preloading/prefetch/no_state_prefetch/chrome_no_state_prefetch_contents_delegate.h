// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_

#include "components/no_state_prefetch/browser/no_state_prefetch_contents_delegate.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {
class WebContents;
}

namespace prerender {

class NoStatePrefetchContents;

class ChromeNoStatePrefetchContentsDelegate
    : public NoStatePrefetchContentsDelegate {
 public:
  // Returns a NoStatePrefetchContents from the given web_contents, if it's used
  // for no-state prefetching. Otherwise returns nullptr. Handles a nullptr
  // input for convenience.
  static NoStatePrefetchContents* FromWebContents(
      content::WebContents* web_contents);

  ChromeNoStatePrefetchContentsDelegate() = default;
  ~ChromeNoStatePrefetchContentsDelegate() override = default;

  // NoStatePrefetchContentsDelegate overrides.
  void OnNoStatePrefetchContentsCreated(
      content::WebContents* web_contents) override;
  void ReleaseNoStatePrefetchContents(
      content::WebContents* web_contents) override;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_
