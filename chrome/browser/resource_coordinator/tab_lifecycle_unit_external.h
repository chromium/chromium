// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

namespace base {
class Time;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

// Interface to control the lifecycle of a tab exposed outside of
// chrome/browser/resource_coordinator/.
class TabLifecycleUnitExternal {
 public:
  // Returns the TabLifecycleUnitExternal associated with |web_contents|, or
  // nullptr if the WebContents is not associated with a tab.
  static TabLifecycleUnitExternal* FromWebContents(
      content::WebContents* web_contents);

  virtual ~TabLifecycleUnitExternal() = default;

  // Returns the WebContents associated with this tab.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns true if this tab can be automatically discarded.
  virtual bool IsAutoDiscardable() const = 0;

  // Allows/disallows this tab to be automatically discarded.
  virtual void SetAutoDiscardable(bool auto_discardable) = 0;

  // Discards the tab.
  virtual bool DiscardTab(mojom::LifecycleUnitDiscardReason reason,
                          uint64_t memory_footprint_estimate = 0) = 0;

  // Returns the state of the LifecycleUnit.
  virtual mojom::LifecycleUnitState GetTabState() const = 0;

  // Last time ticks at which this tab was focused, or Time::Max() if it is
  // currently focused.
  virtual base::Time GetLastFocusedTime() const = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_
