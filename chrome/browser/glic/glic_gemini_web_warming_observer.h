// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_GEMINI_WEB_WARMING_OBSERVER_H_
#define CHROME_BROWSER_GLIC_GLIC_GEMINI_WEB_WARMING_OBSERVER_H_

#include "base/callback_list.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// A tab feature that observes primary main-frame navigations to Gemini Web
// (e.g. gemini.google.com) and triggers heuristic prewarming of the Glic
// container via Performance Manager when `features::kGlicWarmOnGeminiWeb` is
// enabled.
class GlicGeminiWebWarmingObserver : public content::WebContentsObserver {
 public:
  explicit GlicGeminiWebWarmingObserver(tabs::TabInterface* tab);
  GlicGeminiWebWarmingObserver(const GlicGeminiWebWarmingObserver&) = delete;
  GlicGeminiWebWarmingObserver& operator=(const GlicGeminiWebWarmingObserver&) =
      delete;
  ~GlicGeminiWebWarmingObserver() override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

 private:
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  base::CallbackListSubscription tab_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_GEMINI_WEB_WARMING_OBSERVER_H_
