// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_

#include <optional>

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"

using mojom::LifecycleUnitDiscardReason;

namespace content {
class WebContents;
}

namespace resource_coordinator {

// Interface to be notified of changes to the lifetime of tabs.
class TabLifecycleObserver {
 public:
  // Invoked when the lifecycle state of `contents` changes. `previous_state`
  // and `new_state` are the previous and new states. If either is `DISCARDED,
  // `discard_reason` contains the discard reason.
  virtual void OnTabLifecycleStateChange(
      content::WebContents* contents,
      mojom::LifecycleUnitState previous_state,
      mojom::LifecycleUnitState new_state,
      std::optional<LifecycleUnitDiscardReason> discard_reason) {}

  // Invoked when the auto-discardable state of |contents| changes.
  // |is_auto_discardable| indicates whether |contents| can be automatically
  // discarded.
  virtual void OnTabAutoDiscardableStateChange(content::WebContents* contents,
                                               bool is_auto_discardable) {}

 protected:
  virtual ~TabLifecycleObserver() = default;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_OBSERVER_H_
