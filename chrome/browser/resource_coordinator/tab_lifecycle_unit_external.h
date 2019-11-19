// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class TabLifecycleObserver;

// Interface to control the lifecycle of a tab exposed outside of
// chrome/browser/resource_coordinator/.
class TabLifecycleUnitExternal {
 public:
  // Returns the TabLifecycleUnitExternal associated with |web_contents|, or
  // nullptr if the WebContents is not associated with a tab.
  static TabLifecycleUnitExternal* FromWebContents(
      content::WebContents* web_contents);

  // Adds / removes an observer that is notified when the discarded or auto-
  // discardable state of a tab changes.
  static void AddTabLifecycleObserver(TabLifecycleObserver* observer);
  static void RemoveTabLifecycleObserver(TabLifecycleObserver* observer);

  virtual ~TabLifecycleUnitExternal() = default;

  // Returns the WebContents associated with this tab.
  virtual content::WebContents* GetWebContents() const = 0;

  // Whether the tab is playing audio, has played audio recently, is accessing
  // the microphone, is accessing the camera or is being mirrored.
  virtual bool IsMediaTab() const = 0;

  // Returns true if this tab can be automatically discarded.
  virtual bool IsAutoDiscardable() const = 0;

  // Allows/disallows this tab to be automatically discarded.
  virtual void SetAutoDiscardable(bool auto_discardable) = 0;

  // Discards the tab.
  virtual bool DiscardTab() = 0;

  // Returns true if the tab is discarded.
  virtual bool IsDiscarded() const = 0;

  // Returns the number of times that the tab was discarded.
  virtual int GetDiscardCount() const = 0;

  // Returns true if the tab is frozen.
  virtual bool IsFrozen() const = 0;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_EXTERNAL_H_
