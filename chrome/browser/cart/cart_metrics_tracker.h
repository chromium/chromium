// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_METRICS_TRACKER_H_
#define CHROME_BROWSER_CART_CART_METRICS_TRACKER_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"

class CartMetricsTracker : public BrowserListObserver,
                           public TabStripModelObserver {
 public:
  explicit CartMetricsTracker(Browser* browser);
  ~CartMetricsTracker() override;
  // Called to destroy any observers.
  void ShutDown();
  // Gets called when cart with |url| is opened or might be opening soon. Cache
  // the |url| so that it can later be used to decide if a navigation originates
  // from cart module interaction.
  void PrepareToRecordUKM(const GURL& url);
  // TabStripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  std::optional<GURL> last_interacted_url_;
};

#endif  // CHROME_BROWSER_CART_CART_METRICS_TRACKER_H_
