// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_

#include <map>

#include "base/timer/timer.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// This is a collection of observers each of which observe a tab for changes
// which could cause a change in the TabData that would be generated for them.
//
// TODO(mcnee): There is a fair amount of overlap with GlicPinnedTabManager and
// these should be refactored to share common functionality, such as by moving
// more of the implementation into TabDataObserver which is used by both.
// TODO(mcnee): This only observes a subset of the conditions for changes to
// TabData (those handled by TabDataObserver plus changes to tab and window
// activation), enough to support the current use case of the getTabById glic
// API.
class GlicTabDataObserver {
 public:
  GlicTabDataObserver();
  ~GlicTabDataObserver();

  GlicTabDataObserver(const GlicTabDataObserver&) = delete;
  GlicTabDataObserver& operator=(const GlicTabDataObserver&) = delete;

  // Request receiving updates for tab data. The remote receives updates as
  // long as the mojo pipe is open. The pipe is automatically closed when
  // the subscribed tab no longer exists.
  void SubscribeToTabData(int32_t tab_id,
                          mojo::PendingRemote<mojom::TabDataHandler> remote);

  // For internal use.
  void ScheduleCleanupForTab(tabs::TabHandle tab_handle);

 private:
  class TabObserver;

  void DoCleanup();
  void OnTabWillClose(tabs::TabHandle tab_handle);
  void OnTabDataChanged(TabDataChange tab_data);
  void OnObserverUnused(tabs::TabHandle tab_handle);

  std::map<tabs::TabHandle, std::unique_ptr<TabObserver>> observers_;
  std::set<tabs::TabHandle> pending_cleanup_;
  base::OneShotTimer cleanup_timer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_
