// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_OBSERVER_H_
#define CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_OBSERVER_H_

#include <set>

#include "base/observer_list_types.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"

namespace tabs {
class TabInterface;
}

class TabListInterface;

// An observer for events that may occur on a TabListInterface, irrespective of
// platform.
// TODO(https://crbug.com/482392299): Move to a more appropriate directory.
class TabListInterfaceObserver : public base::CheckedObserver {
 public:
  // Called when a new tab is added to the tab list. `tab` is the newly-added
  // tab, and `index` is the index at which it was added. Note that this doesn't
  // necessarily mean `tab` is a newly-created tab; it may have moved from a
  // different tab list.
  // TODO(https://crbug.com/433545116): This may not be called in all situations
  // on Android platforms, such as if a tab that was closed is re-introduced
  // (see also tabClosureUndone() here:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/tabmodel/android/java/src/org/chromium/chrome/browser/tabmodel/TabModelObserver.java;drc=e2bb611334ebe2b1cbe519ff183f5178896b8c55;l=140).
  virtual void OnTabAdded(TabListInterface& tab_list,
                          tabs::TabInterface* tab,
                          int index) {}

  // Called when the active tab changed. `tab` is the new active tab and is
  // never null.
  virtual void OnActiveTabChanged(TabListInterface& tab_list,
                                  tabs::TabInterface* tab) {}

  // Called when a tab is removed from the tab list. This may be the result of
  // detaching the tab for reparenting, or for the tab being closed. `tab` is
  // the removed tab and may be null after this call.  `removed_reason`
  // indicates the reason for the tab removal.
  virtual void OnTabRemoved(TabListInterface& tab_list,
                            tabs::TabInterface* tab,
                            TabRemovedReason removed_reason) {}

  // Called when a tab is moved within the tab list from `from_index` to
  // `to_index`.
  virtual void OnTabMoved(TabListInterface& tab_list,
                          tabs::TabInterface* tab,
                          int from_index,
                          int to_index) {}

  // Called when the set of highlighted tabs changes in the tab list.
  virtual void OnHighlightedTabsChanged(
      TabListInterface& tab_list,
      const std::set<tabs::TabInterface*>& highlighted_tabs) {}

  // Called when the TabListInterface is destroyed.
  virtual void OnTabListDestroyed(TabListInterface& tab_list) {}

  // Called when all tabs in the TabListInterface are closing.
  virtual void OnAllTabsAreClosing(TabListInterface& tab_list) {}
};

#endif  // CHROME_BROWSER_TAB_LIST_TAB_LIST_INTERFACE_OBSERVER_H_
