// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_

#include <memory>

#include "base/time/time.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {
class SessionRestorePolicy;
}

class TabLoaderCallback {
 public:
  // This function will get called to suppress and to allow tab loading. Tab
  // loading is initially enabled.
  virtual void SetTabLoadingEnabled(bool enable_tab_loading) = 0;

  // Invoked by the delegate to inform the tab loader of a change in the score
  // of a tab. A higher score means the tab should be restored sooner.
  virtual void NotifyTabScoreChanged(content::WebContents* contents,
                                     float score) = 0;
};

// TabLoaderDelegate is created once the SessionRestore process is complete and
// the loading of hidden tabs starts.
class TabLoaderDelegate {
 public:
  TabLoaderDelegate(const TabLoaderDelegate&) = delete;
  TabLoaderDelegate& operator=(const TabLoaderDelegate&) = delete;

  virtual ~TabLoaderDelegate() {}

  // Create a tab loader delegate. |TabLoaderCallback::SetTabLoadingEnabled| can
  // get called to disable / enable tab loading.
  // The callback object is valid as long as this object exists.
  static std::unique_ptr<TabLoaderDelegate> Create(TabLoaderCallback* callback);

  // Returns the default timeout time after which the first non-visible tab
  // gets loaded if the first (visible) tab did not finish loading.
  virtual base::TimeDelta GetFirstTabLoadingTimeout() const = 0;

  // Returns the default timeout time after which the next tab gets loaded if
  // the previous tab did not finish loading.
  virtual base::TimeDelta GetTimeoutBeforeLoadingNextTab() const = 0;

  // Returns the maximum number of tabs that should be loading simultaneously.
  virtual size_t GetMaxSimultaneousTabLoads() const = 0;

  // Notifies the delegate of a tab that will be restored. This informs the
  // delegate that this tab is being tracked, and changes in its priority or
  // ranking should be forwarded to the TabLoader. WebContents provided to the
  // delegate via this function are guaranteed to remain valid to derefence
  // until a subsequent RemoveTab. Returns an initial score for the tab.
  virtual float AddTabForScoring(content::WebContents* contents) const = 0;

  // Notifies the delegate of a tab that is no longer being tracked.
  virtual void RemoveTabForScoring(content::WebContents* contents) const = 0;

  // Determines whether or not the given tab should be loaded. If this returns
  // false, then the TabLoader no longer attempts to load |contents| and removes
  // it from TabLoaders internal state. This is called immediately prior to
  // trying to load the tab and allows the TabLoader to respond to changing
  // conditions.
  virtual bool ShouldLoad(content::WebContents* contents) const = 0;

  // Notifies the delegate that a tab load has been initiated.
  virtual void NotifyTabLoadStarted() = 0;

  // Returns the policy engine that is in use.
  virtual resource_coordinator::SessionRestorePolicy* GetPolicyForTesting() = 0;

  // Testing seam to inject a custom SessionRestorePolicy.
  static void SetSessionRestorePolicyForTesting(
      resource_coordinator::SessionRestorePolicy* policy);

 protected:
  // The delegate should only be created via the factory function.
  TabLoaderDelegate() {}
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_LOADER_DELEGATE_H_
