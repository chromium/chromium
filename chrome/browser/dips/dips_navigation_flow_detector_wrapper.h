// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_WRAPPER_H_
#define CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_WRAPPER_H_

#include "base/memory/weak_ptr.h"

class DipsNavigationFlowDetector;
namespace base {
class CallbackListSubscription;
}
namespace content {
class WebContents;
}
namespace tabs {
class TabInterface;
}

// A wrapper around DipsNavigationFlowDetector for registering with a desktop
// tab in chrome/browser/ui/tabs/tab_features.cc.
class DipsNavigationFlowDetectorWrapper {
 public:
  explicit DipsNavigationFlowDetectorWrapper(tabs::TabInterface& tab);
  ~DipsNavigationFlowDetectorWrapper();

  DipsNavigationFlowDetector* GetDetector();

 private:
  // Called when the tab's WebContents is replaced.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  raw_ptr<tabs::TabInterface> tab_;
  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  base::WeakPtrFactory<DipsNavigationFlowDetectorWrapper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_WRAPPER_H_
