// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TAB_STRIP_TRACKER_DESKTOP_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TAB_STRIP_TRACKER_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserWindowInterface;
class BrowserTabStripTracker;
class TabStripModel;
class TabStripModelChange;
struct TabStripSelectionChange;

namespace actor {

class ActorKeyedService;

// This class encapsulates all Desktop-only UI behaviors for ActorKeyedService.
// It tracks tab strip changes and notifies the service.
// The equivalent behavior on Android is implemented in Java (see
// TabModelRemover and ActorKeyedServiceAndroid).
class ActorTabStripTrackerDesktop : public BrowserTabStripTrackerDelegate,
                                    public TabStripModelObserver {
 public:
  explicit ActorTabStripTrackerDesktop(ActorKeyedService& service,
                                       bool enable_tab_tracking = true);
  ~ActorTabStripTrackerDesktop() override;

  ActorTabStripTrackerDesktop(const ActorTabStripTrackerDesktop&) = delete;
  ActorTabStripTrackerDesktop& operator=(const ActorTabStripTrackerDesktop&) =
      delete;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  const raw_ref<ActorKeyedService> service_;
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TAB_STRIP_TRACKER_DESKTOP_H_
