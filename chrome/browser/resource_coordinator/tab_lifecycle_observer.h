// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

using mojom::LifecycleUnitDiscardReason;

namespace content {
class WebContents;
}

namespace resource_coordinator {

// Interface to be notified of changes to the lifetime of tabs.
class TabLifecycleObserver {
 public:
  // Invoked after |contents| is discarded or reloaded after a discard.
  // |is_discarded| indicates if |contents| is currently discarded.
  virtual void OnDiscardedStateChange(content::WebContents* contents,
                                      LifecycleUnitDiscardReason reason,
                                      bool is_discarded) {}

  // Invoked when the auto-discardable state of |contents| changes.
  // |is_auto_discardable| indicates whether |contents| can be automatically
  // discarded.
  virtual void OnAutoDiscardableStateChange(content::WebContents* contents,
                                            bool is_auto_discardable) {}

  // Invoked when a tab is frozen or resumed.
  virtual void OnFrozenStateChange(content::WebContents* contents,
                                   bool is_frozen) {}

 protected:
  virtual ~TabLifecycleObserver() = default;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_
