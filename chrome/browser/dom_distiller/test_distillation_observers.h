// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_DISTILLER_TEST_DISTILLATION_OBSERVERS_H_
#define CHROME_BROWSER_DOM_DISTILLER_TEST_DISTILLATION_OBSERVERS_H_

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"

namespace dom_distiller {

// Helper class that blocks test execution until |observed_contents| enters a
// certain state. Subclasses specify the precise state by calling
// |Stop()| when |observed_contents| is ready.
class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* observed_contents) {
    content::WebContentsObserver::Observe(observed_contents);
  }

  void WaitUntilFinishedLoading() { new_url_loaded_runner_.Run(); }

 protected:
  void Stop() { new_url_loaded_runner_.Quit(); }

 private:
  base::RunLoop new_url_loaded_runner_;
};

class OriginalPageNavigationObserver : public NavigationObserver {
 public:
  using NavigationObserver::NavigationObserver;

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
};

// DistilledPageObserver is used to detect if a distilled page has
// finished loading. This is done by checking how many times the title has
// been set rather than using "DidFinishLoad" directly due to the content
// being set by JavaScript.
class DistilledPageObserver : public NavigationObserver {
 public:
  using NavigationObserver::NavigationObserver;

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void TitleWasSet(content::NavigationEntry* entry) override;

  // DidFinishLoad() can come after the two title settings.
  void MaybeNotifyLoaded();

  int title_set_count_ = 0;
  bool loaded_distiller_page_ = false;
};

}  // namespace dom_distiller

#endif  // CHROME_BROWSER_DOM_DISTILLER_TEST_DISTILLATION_OBSERVERS_H_
