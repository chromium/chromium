// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

// Observes page load completion, first paint, and visibility changes on a tab
// to trigger Default Search Engine prewarm when
// `kPrewarmPageLoadAndTabSwitchAfterPaintTrigger` is enabled. Owned by the
// tab's TabFeatures.
// Note: `PrewarmTabHelper` covers WebContents-rendered pages (including the
// desktop WebUI NTP). On Android, the native Java NTP
// (`chrome-native://newtab`) is not rendered by Blink/WebContents and triggers
// prewarm after its first view draw in `NewTabPage.java`.
class PrewarmTabHelper : public content::WebContentsObserver {
 public:
  static std::unique_ptr<PrewarmTabHelper> MaybeCreate(
      tabs::TabInterface& tab,
      content::WebContents* web_contents);

  PrewarmTabHelper(const PrewarmTabHelper&) = delete;
  PrewarmTabHelper& operator=(const PrewarmTabHelper&) = delete;

  ~PrewarmTabHelper() override;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  PrewarmTabHelper(tabs::TabInterface& tab, content::WebContents* web_contents);

  void MaybeTriggerPrewarm();

  const raw_ref<tabs::TabInterface> tab_;
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_
