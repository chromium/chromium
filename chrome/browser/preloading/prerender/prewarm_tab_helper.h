// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_
#define CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_

#include <memory>

#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Observes page load completion, first paint, and visibility changes on a tab
// to trigger Default Search Engine prewarm when
// `kPrewarmPageLoadAndTabSwitchAfterPaintTrigger` is enabled. Owned by the
// tab's TabFeatures.
// Note: In this CL, `PrewarmTabHelper` is only wired up in desktop
// `TabFeatures` (covering WebContents-backed pages including the desktop WebUI
// NTP). Android `TabFeatures` wiring and Android native Java NTP
// (`chrome-native://newtab`) support are implemented in a follow-up CL.
class PrewarmTabHelper : public content::WebContentsObserver {
 public:
  static std::unique_ptr<PrewarmTabHelper> MaybeCreate(
      content::WebContents* web_contents);

  PrewarmTabHelper(const PrewarmTabHelper&) = delete;
  PrewarmTabHelper& operator=(const PrewarmTabHelper&) = delete;

  ~PrewarmTabHelper() override;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override;
  void DidStopLoading() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  explicit PrewarmTabHelper(content::WebContents* web_contents);

  void MaybeTriggerPrewarm();
};

#endif  // CHROME_BROWSER_PRELOADING_PRERENDER_PREWARM_TAB_HELPER_H_
