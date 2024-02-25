// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SESSIONS_BROWSER_LIST_ROUTER_HELPER_H_
#define CHROME_BROWSER_SYNC_SESSIONS_BROWSER_LIST_ROUTER_HELPER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace sync_sessions {

// Non-android helper of SyncSessionsWebContentsRouter that adds tracking for
// multi-window scenarios(e.g. tab movement between windows). Android doesn't
// have a BrowserList or TabStrip, so it doesn't compile the needed
// dependencies, nor would it benefit from the added tracking.
class BrowserListRouterHelper : public BrowserListObserver,
                                public TabStripModelObserver {
 public:
  explicit BrowserListRouterHelper(SyncSessionsWebContentsRouter* router,
                                   Profile* profile);

  BrowserListRouterHelper(const BrowserListRouterHelper&) = delete;
  BrowserListRouterHelper& operator=(const BrowserListRouterHelper&) = delete;

  ~BrowserListRouterHelper() override;

 private:
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;
  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // |router_| owns |this|.
  const raw_ptr<SyncSessionsWebContentsRouter> router_;

  const raw_ptr<Profile> profile_;

  std::set<raw_ptr<Browser, SetExperimental>> attached_browsers_;
};

}  // namespace sync_sessions

#endif  // CHROME_BROWSER_SYNC_SESSIONS_BROWSER_LIST_ROUTER_HELPER_H_
