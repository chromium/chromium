// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_

#include <map>

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

  using TabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  base::CallbackListSubscription AddTabDataChangedCallback(
      TabDataChangedCallback callback);

  // TODO(mcnee): This is called manually by actor code. Observation should be
  // started in response to the getTabById glic API itself.
  void ObserveTabData(tabs::TabHandle tab_handle);

 private:
  class TabObserver;

  void OnTabWillClose(tabs::TabHandle tab_handle);
  void OnTabDataChanged(TabDataChange tab_data);

  // Callbacks to invoke when the tab data for a tab changes.
  base::RepeatingCallbackList<void(const TabDataChange&)>
      tab_data_changed_callback_list_;

  std::map<tabs::TabHandle, std::unique_ptr<TabObserver>> observers_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_DATA_OBSERVER_H_
