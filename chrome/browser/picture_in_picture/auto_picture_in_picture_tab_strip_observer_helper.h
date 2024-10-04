// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
class AutoPictureInPictureTabStripObserverHelper
    : public TabStripModelObserver {
 public:
  using ActivatedChangedCallback =
      base::RepeatingCallback<void(bool is_tab_activated)>;

  AutoPictureInPictureTabStripObserverHelper(
      const content::WebContents* web_contents,
      ActivatedChangedCallback callback);
  AutoPictureInPictureTabStripObserverHelper(
      const AutoPictureInPictureTabStripObserverHelper&) = delete;
  AutoPictureInPictureTabStripObserverHelper& operator=(
      const AutoPictureInPictureTabStripObserverHelper&) = delete;
  ~AutoPictureInPictureTabStripObserverHelper() override;

  // Begins observing |web_contents_|'s tabstrip.
  void StartObserving();

  // Stops observing |web_contents_|'s tabstrip.
  void StopObserving();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Convenience method to get the WebContents for the active tab, if any.
  content::WebContents* GetActiveWebContents() const;

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

  const raw_ptr<const content::WebContents> web_contents_;
  ActivatedChangedCallback callback_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_STRIP_OBSERVER_HELPER_H_
