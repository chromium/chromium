// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_observer_helper_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}  // namespace content

class BrowserTabStripTracker;

// The AutoPictureInPictureTabStripObserverHelper is used by the
// AutoPictureInPictureTabHelper to observe the current tabstrip of its
// WebContents and notify the AutoPictureInPictureTabHelper whenever the
// WebContents's tab changes from being the active tab on its tabstrip to not
// and vice versa.
class AutoPictureInPictureTabStripObserverHelper final
    : public AutoPictureInPictureTabObserverHelperBase,
      public TabStripModelObserver {
 public:
  AutoPictureInPictureTabStripObserverHelper(content::WebContents* web_contents,
                                             ActivatedChangedCallback callback);
  AutoPictureInPictureTabStripObserverHelper(
      const AutoPictureInPictureTabStripObserverHelper&) = delete;
  AutoPictureInPictureTabStripObserverHelper& operator=(
      const AutoPictureInPictureTabStripObserverHelper&) = delete;
  ~AutoPictureInPictureTabStripObserverHelper() override;

  // AutoPictureInPictureTabObserverHelperBase:
  void StartObserving() override;
  void StopObserving() override;
  content::WebContents* GetActiveWebContents() const override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  void UpdateIsTabActivated(const TabStripModel* tab_strip_model);

  void ObserveTabStripModel(TabStripModel* tab_strip_model);

  TabStripModel* GetCurrentTabStripModel() const;

  // Tracks when browser tab strips change so we can tell when the observed
  // WebContents changes between being the active tab and not being the active
  // tab.
  std::unique_ptr<BrowserTabStripTracker> browser_tab_strip_tracker_;

  raw_ptr<TabStripModel> observed_tab_strip_model_ = nullptr;

  // True if the tab is the activated tab on its tab strip.
  bool is_tab_activated_ = false;

  // True if we're currently observing |web_contents_|'s tabstrip.
  bool is_observing_ = false;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_
